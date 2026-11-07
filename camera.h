#ifndef CAMERA_H
#define CAMERA_H

#include "hittable.h"
#include "material.h"
#include "pdf.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

using ScanlineCallback = std::function<void(int, const std::vector<color>&)>;

class camera {
    public:
        double aspect_ratio = 1.0;
        int image_width = 100;
        int samples_per_pixel = 10;
        int max_depth = 10;
        color background;

        double vfov = 90;
        point3 lookfrom = point3(0, 0, 0);
        point3 lookat = point3(0, 0, -1);
        vec3 vup = vec3(0, 1, 0);

        double defocus_angle = 0;
        double focus_dist = 10;

        int tile_size = 32;
        int thread_count = 0;
        color tile_outline = color(0.35, 0.35, 0.35);

        void render(const hittable& world, const hittable& lights, ScanlineCallback on_scanline) {
            using namespace std::chrono_literals;
            initialize();

            struct rect { int x0, y0, w, h; };
            std::vector<rect> tiles;
            for (int y = 0; y < image_height; y += tile_size)
                for (int x = 0; x < image_width; x += tile_size)
                    tiles.push_back(
                        {x, y, std::min(tile_size, image_width - x), std::min(tile_size, image_height - y)}
                    );

            const double cx = image_width / 2.0, cy = image_height / 2.0;
            auto dist2 = [&](const rect& t) {
                double dx = t.x0 + t.w / 2.0 - cx, dy = t.y0 + t.h / 2.0 - cy;
                return dx * dx + dy * dy;
            };
            std::sort(tiles.begin(), tiles.end(), [&](const rect& a, const rect& b) { return dist2(a) < dist2(b); });

            const int n_tiles = static_cast<int>(tiles.size());
            const unsigned n_threads = thread_count > 0 ? static_cast<unsigned>(thread_count) : std::max(1u, std::thread::hardware_concurrency() - 1);
            std::clog << "Threads: " << n_threads << ", tiles: " << n_tiles << " (" << tile_size << "x" << tile_size << ")\n";

            std::vector<color> frame(static_cast<size_t>(image_width) * image_height);
            std::vector<std::atomic<int>> rows_done(n_tiles);
            std::vector<std::atomic<bool>> started(n_tiles);
            std::atomic<int> next_tile{0};
            std::atomic<int> tiles_finished{0};

            auto worker = [&] {
                for (int k; (k = next_tile++) < n_tiles; ) {
                    const rect& t = tiles[k];
                    started[k].store(true, std::memory_order_release);

                    for (int j = 0; j < t.h; j++) {
                        for (int i = 0; i < t.w; i++) {
                            color pixel_color(0, 0, 0);
                            for (int s_j = 0; s_j < sqrt_spp; s_j++) {
                                for (int s_i = 0; s_i < sqrt_spp; s_i++) {
                                    ray r = get_ray(t.x0 + i, t.y0 + j, s_i, s_j);
                                    pixel_color += ray_color(r, max_depth, world, lights);
                                }
                            }
                            frame[static_cast<size_t>(t.y0 + j) * image_width + t.x0 + i] = pixel_samples_scale * pixel_color;
                        }
                        rows_done[k].store(j + 1, std::memory_order_release);
                    }
                    tiles_finished.fetch_add(1, std::memory_order_release);
                }
            };

            std::vector<std::thread> threads;
            for (unsigned n = 0; n < n_threads; n++)
                threads.emplace_back(worker);

            std::vector<std::vector<color>> shown(image_height, std::vector<color>(image_width, color(0, 0, 0)));
            std::vector<int>  rows_shown(n_tiles, 0);
            std::vector<char> outlined(n_tiles, 0);
            std::vector<char> dirty(image_height, 0);
            auto last_update = std::chrono::steady_clock::now();

            while (true) {
                const int finished = tiles_finished.load(std::memory_order_acquire);

                for (int k = 0; k < n_tiles; k++) {
                    const rect& t = tiles[k];
                    if (!outlined[k] && started[k].load(std::memory_order_acquire)) {
                        outlined[k] = 1;
                        for (int j = rows_shown[k]; j < t.h; j++) {
                            auto& row = shown[t.y0 + j];
                            if (j == 0 || j == t.h - 1)
                                std::fill_n(row.begin() + t.x0, t.w, tile_outline);
                            row[t.x0] = row[t.x0 + t.w - 1] = tile_outline;
                            dirty[t.y0 + j] = 1;
                        }
                    }

                    const int done = rows_done[k].load(std::memory_order_acquire);
                    for (int j = rows_shown[k]; j < done; j++) {
                        const auto* src = &frame[static_cast<size_t>(t.y0 + j) * image_width + t.x0];
                        std::copy_n(src, t.w, shown[t.y0 + j].begin() + t.x0);
                        dirty[t.y0 + j] = 1;
                    }
                    rows_shown[k] = done;
                }

                bool sent = false;
                for (int y = 0; y < image_height; y++) {
                    if (!dirty[y]) continue;
                    on_scanline(y, shown[y]);
                    dirty[y] = 0;
                    sent = true;
                }

                const auto now = std::chrono::steady_clock::now();
                if (sent) {
                    last_update = now;
                } else if (now - last_update > 100ms) {
                    on_scanline(0, shown[0]);
                    last_update = now;
                }

                std::clog << "\rTiles: " << finished << " / " << n_tiles << "   " << std::flush;

                if (finished == n_tiles) break;
                std::this_thread::sleep_for(30ms);
            }

            for (auto& th : threads) th.join();
            std::clog << "\rDone. \n";
        }

    private:
        int image_height;
        double pixel_samples_scale;
        int sqrt_spp;
        double recip_sqrt_spp;
        point3 center;
        point3 pixel00_loc;
        vec3 pixel_delta_u;
        vec3 pixel_delta_v;
        vec3 u, v, w;
        vec3 defocus_disk_u;
        vec3 defocus_disk_v;

        void initialize() {
            image_height = int(image_width / aspect_ratio);
            image_height = (image_height < 1) ? 1 : image_height;

            sqrt_spp = int(std::sqrt(samples_per_pixel));
            pixel_samples_scale = 1.0 / (sqrt_spp * sqrt_spp);
            recip_sqrt_spp = 1.0 / sqrt_spp;

            center = lookfrom;

            auto theta = degrees_to_radians(vfov);
            auto h = std::tan(theta / 2);
            auto viewport_height = 2 * h * focus_dist;
            auto viewport_width = viewport_height * (double(image_width)/image_height);

            w = unit_vector(lookfrom - lookat);
            u = unit_vector(cross(vup, w));
            v = cross(w, u);

            vec3 viewport_u = viewport_width * u;
            vec3 viewport_v = viewport_height * -v;

            pixel_delta_u = viewport_u / image_width;
            pixel_delta_v = viewport_v / image_height;

            auto viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
            pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

            auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
            defocus_disk_u = u * defocus_radius;
            defocus_disk_v = v * defocus_radius;
        }

        ray get_ray(int i, int j, int s_i, int s_j) const {
            auto offset = sample_square_stratified(s_i, s_j);
            auto pixel_sample = pixel00_loc
                                + ((i + offset.x()) * pixel_delta_u)
                                + ((j + offset.y()) * pixel_delta_v);

            auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
            auto ray_direction = pixel_sample - ray_origin;
            auto ray_time = random_double();

            return ray(ray_origin, ray_direction, ray_time);
        }

        vec3 sample_square_stratified(int s_i, int s_j) const {
            auto px = ((s_i + random_double()) * recip_sqrt_spp) - 0.5;
            auto py = ((s_j + random_double()) * recip_sqrt_spp) - 0.5;

            return vec3(px, py, 0);
        }

        vec3 sample_square() const {
            return vec3(random_double() - 0.5, random_double() - 0.5, 0);
        }

        point3 defocus_disk_sample() const {
            auto p = random_in_unit_disk();
            return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
        }

        color ray_color(const ray& r, int depth, const hittable& world, const hittable& lights) const {
            if (depth <= 0)
                return color(0, 0, 0);

            hit_record rec;

            if (!world.hit(r, interval(0.001, infinity), rec))
                return background;

            scatter_record srec;
            color color_from_emission = rec.mat->emitted(r, rec, rec.u, rec.v, rec.p);

            if (!rec.mat->scatter(r, rec, srec))
                return color_from_emission;

            if (srec.skip_pdf) {
                return srec.attenuation * ray_color(srec.skip_pdf_ray, depth-1, world, lights);
            }

            auto p0 = make_shared<hittable_pdf>(lights, rec.p);
            auto p1 = make_shared<cosine_pdf>(rec.normal);
            mixture_pdf mixed_pdf(p0, p1);

            auto light_ptr = make_shared<hittable_pdf>(lights, rec.p);
            mixture_pdf p(light_ptr, srec.pdf_ptr);

            ray scattered = ray(rec.p, p.generate(), r.time());
            auto pdf_value = p.value(scattered.direction());

            double scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);

            color sample_color = ray_color(scattered, depth-1, world, lights);
            color color_from_scatter = (srec.attenuation * scattering_pdf * sample_color) / pdf_value;
            return color_from_emission + color_from_scatter;
        }
};

#endif