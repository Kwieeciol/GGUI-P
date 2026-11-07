#ifndef CAMERA_H
#define CAMERA_H

#include "hittable.h"
#include "material.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

struct tile {
    int x0, y0;
    int width, height;
    std::vector<color> pixels;
};

using ScanlineCallback = std::function<void(int, const std::vector<color>&)>;
using TileCallback = std::function<void(const tile&)>;

class camera {
    public:
        double aspect_ratio = 1.0;
        int image_width = 100;
        int samples_per_pixel = 10;
        int max_depth = 10;
        color  background;

        double vfov = 90; 
        point3 lookfrom = point3(0, 0, 0);
        point3 lookat = point3(0, 0, -1);
        vec3 vup = vec3(0, 1, 0);

        double defocus_angle = 0;
        double focus_dist = 10;

        int tile_size = 32;
        int thread_count = 0; // 0 -wszystkie watki cpu

        void render_tiles(const hittable& world, TileCallback on_tile) {
            initialize();

            std::vector<tile> tiles;
            for (int y = 0; y < image_height; y += tile_size) {
                for (int x = 0; x < image_width; x += tile_size) {
                    tiles.push_back({
                        x, y, std::min(tile_size, image_width - x), std::min(tile_size, image_height - y), {}
                    });
                }
            }

            const double cx = image_width / 2.0, cy = image_height / 2.0;
            auto dist2 = [&](const tile& t) {
                double dx = t.x0 + t.width / 2.0 - cx;
                double dy = t.y0 + t.height / 2.0 - cy;
                return dx * dx + dy * dy;
            };
            std::sort(tiles.begin(), tiles.end(), [&](const tile& a, const tile& b) {
                return dist2(a) < dist2(b);
            });

            const int n_tiles = static_cast<int>(tiles.size());
            const unsigned n_threads = thread_count > 0 ? static_cast<unsigned>(thread_count) : std::max(1u, std::thread::hardware_concurrency());

            std::clog << "Threads: " << n_threads << ", tiles: " << n_tiles << " (" << tile_size << "x" << tile_size << ")\n";

            std::atomic<int> next_tile{0};
            std::mutex m;
            std::condition_variable cv;
            std::vector<int> finished;

            auto worker = [&] {
                for (int k; (k = next_tile++) < n_tiles; ) {
                    tile& t = tiles[k];
                    t.pixels.resize(static_cast<size_t>(t.width) * t.height);

                    for (int j = 0; j < t.height; j++) {
                        for (int i = 0; i < t.width; i++) {
                            color pixel_color(0, 0, 0);
                            for (int sample = 0; sample < samples_per_pixel; sample++) {
                                ray r = get_ray(t.x0 + i, t.y0 + j);
                                pixel_color += ray_color(r, max_depth, world);
                            }
                            t.pixels[j * t.width + i] = pixel_samples_scale * pixel_color;
                        }
                    }

                    { std::lock_guard lock(m); finished.push_back(k); }
                    cv.notify_one();
                }
            };

            std::vector<std::thread> threads;
            for (unsigned n = 0; n < n_threads; n++) {
                threads.emplace_back(worker);
            }

            for (int done = 0; done < n_tiles; ) {
                std::vector<int> batch;
                {
                    std::unique_lock lock(m);
                    cv.wait(lock, [&] { return !finished.empty(); });
                    batch.swap(finished);
                }
                for (int k : batch) {
                    on_tile(tiles[k]);
                    tiles[k].pixels = {};
                    done++;
                }
                std::clog << "\rTiles: " << done << " / " << n_tiles << " " << std::flush;
            }

            for (auto& t : threads) t.join();
            std::clog << "\rDone.\n";
        }

        void render(const hittable& world, ScanlineCallback on_scanline) {
            std::vector<std::vector<color>> image;
 
            render_tiles(world, [&](const tile& t) {
                if (image.empty())
                    image.assign(image_height, std::vector<color>(image_width, color(0, 0, 0)));
 
                for (int j = 0; j < t.height; j++)
                    std::copy_n(t.pixels.begin() + j * t.width, t.width,
                                image[t.y0 + j].begin() + t.x0);
 
                for (int j = 0; j < t.height; j++)
                    on_scanline(t.y0 + j, image[t.y0 + j]);
            });
        }
            
    private:
        int image_height;
        double pixel_samples_scale;
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

            pixel_samples_scale = 1.0 / samples_per_pixel;

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

        ray get_ray(int i, int j) const {
            auto offset = sample_square();
            auto pixel_sample = pixel00_loc
                                + ((i + offset.x()) * pixel_delta_u)
                                + ((j + offset.y()) * pixel_delta_v);

            auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
            auto ray_direction = pixel_sample - ray_origin;
            auto ray_time = random_double();

            return ray(ray_origin, ray_direction, ray_time);
        }

        vec3 sample_square() const {
            return vec3(random_double() - 0.5, random_double() - 0.5, 0);
        }

        point3 defocus_disk_sample() const {
            auto p = random_in_unit_disk();
            return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
        }

        color ray_color(const ray& r, int depth, const hittable& world) const {
            if (depth <= 0)
                return color(0, 0, 0);

            hit_record rec;

            if (!world.hit(r, interval(0.001, infinity), rec))
                return background;

            ray scattered;
            color attenuation;
            color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);

            if (!rec.mat->scatter(r, rec, attenuation, scattered))
                return color_from_emission;

            color color_from_scatter = attenuation * ray_color(scattered, depth-1, world);

            return color_from_emission + color_from_scatter;
        }
};

#endif