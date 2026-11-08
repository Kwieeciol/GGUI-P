#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #define popen  _popen
    #define pclose _pclose
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#endif

#include "color.h"
#include "render_progress.h"

namespace fs = std::filesystem;

inline fs::path executable_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    std::uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0)
        return fs::canonical(buf).parent_path();
    return fs::current_path();
#else
    return fs::current_path();
#endif
}

inline fs::path ffmpeg_path() {
#ifdef _WIN32
    return executable_dir() / "ffmpeg.exe";
#else
    return executable_dir() / "ffmpeg";
#endif
}


struct ExportFormat {
    const char* name;
    const char* ext;
    bool has_quality; // shows the quality slider
    bool has_lossless; // shows the lossless checkbox
};

inline constexpr std::array<ExportFormat, 6> export_formats{{
    {"PNG",  "png",  false, false},
    {"JPEG", "jpg",  true,  false},
    {"WebP", "webp", true,  true },
    {"AVIF", "avif", true,  false},
    {"TIFF", "tiff", false, false},
    {"BMP",  "bmp",  false, false},
}};

struct ExportOptions {
    int format   = 0; // index into export_formats
    int quality  = 90; // 1..100
    int scale    = 100; // percent
    bool lossless = false;
};


class Visualizer {
private:
    static constexpr unsigned menu_height = 20;
    static constexpr unsigned status_height = 24;

    unsigned width;
    unsigned height;

    sf::RenderWindow window;
    sf::Image image;
    sf::Texture texture;
    sf::Sprite sprite;
    sf::Clock delta_clock;
    sf::Clock present_clock;

    ExportOptions options;
    bool has_ffmpeg = fs::exists(ffmpeg_path());

    char name_buf[256] = "render";
    std::string status;
    bool request_export = false;

    static std::uint8_t to_byte(double v) {
        return static_cast<std::uint8_t>(256.0 * std::clamp(linear_to_gamma(v), 0.0, 0.999));
    }

    static std::string quote(const fs::path& p) { return "\"" + p.string() + "\""; }

    unsigned out_width()  const { return std::max(1u, width  * options.scale / 100); }
    unsigned out_height() const { return std::max(1u, height * options.scale / 100); }

    std::string ffmpeg_args() const {
        const ExportFormat& f = export_formats[options.format];
        const int q = options.quality;
        std::string a;

        if (options.scale != 100)
            a += " -vf scale=" + std::to_string(out_width()) + ":" +
                 std::to_string(out_height()) + ":flags=lanczos";

        const std::string ext = f.ext;
        if (ext == "jpg") {
            a += " -q:v " + std::to_string(2 + (100 - q) * 29 / 100);
        } else if (ext == "webp") {
            a += " -c:v libwebp";
            a += options.lossless ? " -lossless 1" : " -quality " + std::to_string(q);
        } else if (ext == "avif") {
            a += " -c:v libaom-av1 -still-picture 1 -pix_fmt yuv444p -b:v 0"
                 " -crf " + std::to_string((100 - q) * 63 / 100);
        } else if (ext == "tiff") {
            a += " -compression_algo deflate";
        }
        return a;
    }

    static int run_command(std::string cmd, std::string& output) {
        cmd += " 2>&1";
#ifdef _WIN32
        cmd = "\"" + cmd + "\"";
#endif
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return -1;

        char buf[256];
        while (std::fgets(buf, sizeof(buf), pipe))
            output += buf;
        return pclose(pipe);
    }

    bool export_image(fs::path out) {
        const ExportFormat& f = export_formats[options.format];
        out.replace_extension(f.ext);

        if (std::string(f.ext) == "png" && options.scale == 100) {
            if (!image.saveToFile(out)) { status = "Failed to save " + out.string(); return false; }
            status = "Exported " + out.string();
            return true;
        }

        if (!has_ffmpeg) {
            status = "ffmpeg not found next to the executable";
            return false;
        }

        const fs::path tmp = fs::temp_directory_path() / "raytracer_export_tmp.png";
        if (!image.saveToFile(tmp)) {
            status = "Could not write temporary file";
            return false;
        }

        const std::string cmd =
            quote(ffmpeg_path()) + " -y -hide_banner -loglevel error"
            " -i " + quote(tmp) + ffmpeg_args() + " -frames:v 1 " + quote(out);

        std::string output;
        const int code = run_command(cmd, output);

        std::error_code ec;
        fs::remove(tmp, ec);

        if (code != 0) {
            const auto line_end = output.find('\n');
            status = "ffmpeg failed: " + output.substr(0, line_end);
            return false;
        }

        status = "Exported " + out.string();
        return true;
    }


    void handle_events() {
        while (const auto event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                apply_letterbox(resized->size);
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                const bool cmd = key->control || key->system;
                if (cmd && key->code == sf::Keyboard::Key::E &&
                    !ImGui::GetIO().WantCaptureKeyboard)
                    request_export = true;
            }
        }
    }

    void draw_options_menu() {
        if (!ImGui::BeginMenu("Options")) return;

        ImGui::SeparatorText("Export");
        ImGui::PushItemWidth(160);

        const ExportFormat& current = export_formats[options.format];
        if (ImGui::BeginCombo("Format", current.name)) {
            for (int i = 0; i < static_cast<int>(export_formats.size()); i++) {
                const bool needs_ffmpeg = !(i == 0 && options.scale == 100);
                ImGui::BeginDisabled(needs_ffmpeg && !has_ffmpeg);
                if (ImGui::Selectable(export_formats[i].name, options.format == i))
                    options.format = i;
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }

        const ExportFormat& f = export_formats[options.format];
        if (f.has_lossless)
            ImGui::Checkbox("Lossless", &options.lossless);

        if (f.has_quality) {
            ImGui::BeginDisabled(f.has_lossless && options.lossless);
            ImGui::SliderInt("Quality", &options.quality, 1, 100);
            ImGui::EndDisabled();
        }

        ImGui::SliderInt("Scale", &options.scale, 10, 400, "%d%%");
        ImGui::TextDisabled("Output: %u x %u", out_width(), out_height());

        ImGui::PopItemWidth();

        if (!has_ffmpeg) {
            ImGui::Separator();
            ImGui::TextColored({1.f, 0.6f, 0.2f, 1.f}, "ffmpeg not found - only PNG at 100%%");
        }

        ImGui::EndMenu();
    }

    void draw_export_popup() {
        if (request_export) {
            ImGui::OpenPopup("Export");
            request_export = false;
        }

        if (!ImGui::BeginPopupModal("Export", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        const ExportFormat& f = export_formats[options.format];

        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        bool confirm = ImGui::InputText("File name", name_buf, sizeof(name_buf),
                                        ImGuiInputTextFlags_EnterReturnsTrue);

        fs::path preview = name_buf;
        preview.replace_extension(f.ext);
        ImGui::TextDisabled("%s, %u x %u  (change in Options)",
                            preview.string().c_str(), out_width(), out_height());

        confirm |= ImGui::Button("Export");
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        if (confirm) {
            export_image(name_buf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void draw_ui() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export...", "Ctrl+E"))
                    request_export = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
                    window.close();
                ImGui::EndMenu();
            }

            draw_options_menu();

            if (!status.empty()) {
                ImGui::Separator();
                ImGui::TextUnformatted(status.c_str());
            }
            ImGui::EndMainMenuBar();
        }

        draw_export_popup();
        draw_status_bar();
    }

    static std::string format_time(double seconds) {
        const int s = static_cast<int>(seconds + 0.5);
        char buf[32];
        if (s >= 3600) std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
        else           std::snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
        return buf;
    }

    void draw_status_bar() {
        const auto win = window.getSize();
        ImGui::SetNextWindowPos({0.f, float(win.y) - float(status_height)});
        ImGui::SetNextWindowSize({float(win.x), float(status_height)});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.f, 4.f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("##status_bar", nullptr, flags)) {
            const RenderProgress& p = render_progress;
            const double elapsed = p.elapsed_seconds();

            if (!p.running && !p.finished) {
                ImGui::TextDisabled("Idle");
            } else if (p.finished) {
                ImGui::Text("Done  |  %d passes  |  %d spp  |  %d threads  |  %s",
                            p.n_passes, p.spp_total, p.threads, format_time(elapsed).c_str());
            } else {
                ImGui::Text("Pass %d/%d", p.pass, p.n_passes);
                ImGui::SameLine(); ImGui::TextDisabled("|");
                ImGui::SameLine(); ImGui::Text("%d/%d spp", p.spp, p.spp_total);
                ImGui::SameLine(); ImGui::TextDisabled("|");
                ImGui::SameLine(); ImGui::Text("Tiles %d/%d", p.tiles_done, p.n_tiles);
                ImGui::SameLine(); ImGui::TextDisabled("|");

                char overlay[32];
                ImGui::SameLine(); ImGui::TextUnformatted("Pass");
                std::snprintf(overlay, sizeof(overlay), "%.0f%%", p.pass_fraction * 100.0);
                ImGui::SameLine(); ImGui::ProgressBar(float(p.pass_fraction), {90.f, 0.f}, overlay);

                ImGui::SameLine(); ImGui::TextUnformatted("Total");
                std::snprintf(overlay, sizeof(overlay), "%.1f%%", p.total_fraction * 100.0);
                ImGui::SameLine(); ImGui::ProgressBar(float(p.total_fraction), {90.f, 0.f}, overlay);

                ImGui::SameLine(); ImGui::TextDisabled("|");
                std::string eta = "--:--";
                if (p.total_fraction > 0.01)
                    eta = format_time(elapsed / p.total_fraction - elapsed);
                ImGui::SameLine(); ImGui::Text("%s  (ETA %s)", format_time(elapsed).c_str(), eta.c_str());
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(4);
    }

    void render_frame() {
        ImGui::SFML::Update(window, delta_clock.restart());
        draw_ui();

        window.clear();
        window.draw(sprite);
        ImGui::SFML::Render(window);
        window.display();
    }

    void apply_letterbox(sf::Vector2u window_size) {
        const float win_w = float(window_size.x);
        const float win_h = float(window_size.y);
        const float menu  = float(menu_height);

        const float area_h = std::max(1.f, win_h - menu - float(status_height));

        const float image_ratio = float(width) / float(height);
        const float area_ratio  = win_w / area_h;

        float draw_w = win_w, draw_h = area_h;
        if (area_ratio > image_ratio) {
            draw_w = area_h * image_ratio;
        } else {
            draw_h = win_w / image_ratio;
        }
        const float x = (win_w - draw_w) / 2.f;
        const float y = menu + (area_h - draw_h) / 2.f;

        sf::View view(sf::FloatRect({0.f, menu}, {float(width), float(height)}));
        view.setViewport(sf::FloatRect({x / win_w, y / win_h}, {draw_w / win_w, draw_h / win_h}));
        window.setView(view);
    }

public:
    Visualizer(unsigned image_width, double aspect_ratio)
        : width(image_width),
          height(std::max(1u, static_cast<unsigned>(image_width / aspect_ratio))),
          window(sf::VideoMode({width, height + menu_height + status_height}), "Ray Tracer"),
          image({width, height}, sf::Color::Black),
          texture(sf::Vector2u{width, height}),
          sprite(texture)
    {
        if (!ImGui::SFML::Init(window))
            throw std::runtime_error("ImGui-SFML init failed");

        sprite.setPosition({0.f, static_cast<float>(menu_height)});
        apply_letterbox(window.getSize());
    }

    ~Visualizer() { ImGui::SFML::Shutdown(); }

    Visualizer(const Visualizer&) = delete;
    Visualizer& operator=(const Visualizer&) = delete;

    void on_scanline(int y, const std::vector<color>& row) {
        if (!window.isOpen()) return;

        for (unsigned x = 0; x < row.size() && x < width; x++) {
            const auto& c = row[x];
            image.setPixel({x, static_cast<unsigned>(y)},
                           sf::Color(to_byte(c.x()), to_byte(c.y()), to_byte(c.z())));
        }

        if (present_clock.getElapsedTime() < sf::milliseconds(16)) return;
        present_clock.restart();

        texture.update(image);
        handle_events();
        render_frame();
    }

    void display() {
        texture.update(image);
        window.setFramerateLimit(60);
        while (window.isOpen()) {
            handle_events();
            render_frame();
        }
    }

    unsigned get_height() const { return height; }
    unsigned get_width() const { return width; }
    const sf::Image& get_image() const { return image; }
};

#endif