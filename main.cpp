#include "rtweekend.h"
#include <chrono>
#include <cstdlib>

#include "bvh.h"
#include "hittable.h"
#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"
#include "quad.h"
#include "visualizer.h"
#include "constant_medium.h"
#include "triangle.h"

void bounding_spheres() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));
    world.add(make_shared<sphere>(point3(0,-1000,0), 1000, make_shared<lambertian>(checker)));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>(albedo);
                    auto center2 = center + vec3(0, random_double(0, .5), 0);
                    world.add(make_shared<sphere>(center, center2, 0.2, sphere_material));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                } else {
                    // glass
                    sphere_material = make_shared<dielectric>(1.5);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

    auto material2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, material3));

    world = hittable_list(make_shared<bvh_node>(world));

    hittable_list lights; // no light sources in this scene

    camera cam;
    Visualizer visualizer(1920, 16.0 / 9.0);

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1920;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = point3(13,2,3);
    cam.lookat = point3(0,0,0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void checkered_spheres() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));

    world.add(make_shared<sphere>(point3(0, -10, 0), 10, make_shared<lambertian>(checker)));
    world.add(make_shared<sphere>(point3(0, 10, 0), 10, make_shared<lambertian>(checker)));

    hittable_list lights; // no light sources in this scene

    camera cam;
    Visualizer visualizer(1920, 16.0 / 9.0);

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1920;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void earth() {
    auto earth_texture = make_shared<image_texture>("earthmap.jpg");
    auto earth_surface = make_shared<lambertian>(earth_texture);
    auto globe = make_shared<sphere>(point3(0,0,0), 2, earth_surface);

    hittable_list lights; // no light sources in this scene

    camera cam;
    Visualizer visualizer(2560, 16.0 / 9.0);

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 2560;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = point3(0,0,12);
    cam.lookat = point3(0,0,0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(hittable_list(globe), lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void perlin_spheres() {
    hittable_list world;

    auto pertext = make_shared<noise_texture>(4);
    world.add(make_shared<sphere>(point3(0,-1000,0), 1000, make_shared<lambertian>(pertext)));
    world.add(make_shared<sphere>(point3(0,2,0), 2, make_shared<lambertian>(pertext)));

    hittable_list lights; // no light sources in this scene

    camera cam;
    Visualizer visualizer(2560, 16.0 / 9.0);

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 2560;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = point3(13,2,3);
    cam.lookat = point3(0,0,0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void quads() {
    hittable_list world;

    auto left_red = make_shared<lambertian>(color(1.0, 0.2, 0.2));
    auto back_green = make_shared<lambertian>(color(0.2, 1.0, 0.2));
    auto right_blue = make_shared<lambertian>(color(0.2, 0.2, 1.0));
    auto upper_orange = make_shared<lambertian>(color(1.0, 0.5, 0.0));
    auto lower_teal = make_shared<lambertian>(color(0.2, 0.8, 0.8));

    world.add(make_shared<quad>(point3(-3,-2, 5), vec3(0, 0,-4), vec3(0, 4, 0), left_red));
    world.add(make_shared<quad>(point3(-2,-2, 0), vec3(4, 0, 0), vec3(0, 4, 0), back_green));
    world.add(make_shared<quad>(point3( 3,-2, 1), vec3(0, 0, 4), vec3(0, 4, 0), right_blue));
    world.add(make_shared<quad>(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4), upper_orange));
    world.add(make_shared<quad>(point3(-2,-3, 5), vec3(4, 0, 0), vec3(0, 0,-4), lower_teal));

    hittable_list lights; // no light sources in this scene

    camera cam;
    Visualizer visualizer(1920, 1.0);

    cam.aspect_ratio = 1.0;
    cam.image_width = 1920;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.7, 0.8, 1.0);

    cam.vfov = 80;
    cam.lookfrom = point3(0,0,9);
    cam.lookat = point3(0,0,0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void cornell_box() {
    hittable_list world;

    auto red = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(15, 15, 15));

    world.add(make_shared<quad>(point3(555,0,0), vec3(0,555,0), vec3(0,0,555), green));
    world.add(make_shared<quad>(point3(0,0,0), vec3(0,555,0), vec3(0,0,555), red));
    world.add(make_shared<quad>(point3(343, 554, 332), vec3(-130,0,0), vec3(0,0,-105), light));
    world.add(make_shared<quad>(point3(0,0,0), vec3(555,0,0), vec3(0,0,555), white));
    world.add(make_shared<quad>(point3(555,555,555), vec3(-555,0,0), vec3(0,0,-555), white));
    world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,555,0), white));

    auto empty_material = shared_ptr<material>();
    hittable_list lights;
    lights.add(
        make_shared<quad>(point3(343,554,332), vec3(-130,0,0), vec3(0,0,-105), empty_material));
    lights.add(make_shared<sphere>(point3(190, 90, 190), 90, empty_material));

    shared_ptr<hittable> box1 = box(point3(0,0,0), point3(165,330,165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265,0,295));
    world.add(box1);

    auto glass = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(190,90,190), 90, glass));

    camera cam;
    Visualizer visualizer(1080, 1.0);

    cam.aspect_ratio = 1.0;
    cam.image_width = 1080;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0,0,0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void simple_light() {
    hittable_list world;

    auto pertext = make_shared<noise_texture>(4);
    world.add(make_shared<sphere>(point3(0,-1000,0), 1000, make_shared<lambertian>(pertext)));
    world.add(make_shared<sphere>(point3(0,2,0), 2, make_shared<lambertian>(pertext)));
    
    auto difflight = make_shared<diffuse_light>(color(4,4,4));
    world.add(make_shared<sphere>(point3(0,7,0), 2, difflight));
    world.add(make_shared<quad>(point3(3,1,-2), vec3(2,0,0), vec3(0,2,0), difflight));

    auto empty_material = shared_ptr<material>();
    quad lights(point3(3,1,-2), vec3(2,0,0), vec3(0,2,0), empty_material);

    camera cam;
    Visualizer visualizer(2560, 16.0 / 9.0);

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 2560;
    cam.samples_per_pixel = 1000;
    cam.max_depth = 50;
    cam.background = color(0,0,0);

    cam.vfov = 20;
    cam.lookfrom = point3(26,3,6);
    cam.lookat = point3(0,2,0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void cornell_smoke() {
    hittable_list world;

    auto red   = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(7, 7, 7));

    world.add(make_shared<quad>(point3(555,0,0), vec3(0,555,0), vec3(0,0,555), green));
    world.add(make_shared<quad>(point3(0,0,0), vec3(0,555,0), vec3(0,0,555), red));
    world.add(make_shared<quad>(point3(113,554,127), vec3(330,0,0), vec3(0,0,305), light));
    world.add(make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555), white));
    world.add(make_shared<quad>(point3(0,0,0), vec3(555,0,0), vec3(0,0,555), white));
    world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,555,0), white));

    shared_ptr<hittable> box1 = box(point3(0,0,0), point3(165,330,165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265,0,295));

    shared_ptr<hittable> box2 = box(point3(0,0,0), point3(165,165,165), white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, vec3(130,0,65));

    world.add(make_shared<constant_medium>(box1, 0.01, color(0,0,0)));
    world.add(make_shared<constant_medium>(box2, 0.01, color(1,1,1)));

    auto empty_material = shared_ptr<material>();
    quad lights(point3(113,554,127), vec3(330,0,0), vec3(0,0,305), empty_material);

    camera cam;
    Visualizer visualizer(1080, 1.0);

    cam.aspect_ratio = 1.0;
    cam.image_width = 1080;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = color(0,0,0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    hittable_list boxes1;
    auto ground = make_shared<lambertian>(color(0.48, 0.83, 0.53));

    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i*w;
            auto z0 = -1000.0 + j*w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1,101);
            auto z1 = z0 + w;

            boxes1.add(box(point3(x0,y0,z0), point3(x1,y1,z1), ground));
        }
    }

    hittable_list world;

    world.add(make_shared<bvh_node>(boxes1));

    auto light = make_shared<diffuse_light>(color(7, 7, 7));
    world.add(make_shared<quad>(point3(123,554,147), vec3(300,0,0), vec3(0,0,265), light));

    auto empty_material = shared_ptr<material>();
    quad lights(point3(123,554,147), vec3(300,0,0), vec3(0,0,265), empty_material);

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30,0,0);
    auto sphere_material = make_shared<lambertian>(color(0.7, 0.3, 0.1));
    world.add(make_shared<sphere>(center1, center2, 50, sphere_material));

    world.add(make_shared<sphere>(point3(260, 150, 45), 50, make_shared<dielectric>(1.5)));
    world.add(make_shared<sphere>(
        point3(0, 150, 145), 50, make_shared<metal>(color(0.8, 0.8, 0.9), 1.0)
    ));

    auto boundary = make_shared<sphere>(point3(360,150,145), 70, make_shared<dielectric>(1.5));
    world.add(boundary);
    world.add(make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));
    boundary = make_shared<sphere>(point3(0,0,0), 5000, make_shared<dielectric>(1.5));
    world.add(make_shared<constant_medium>(boundary, .0001, color(1,1,1)));

    auto emat = make_shared<lambertian>(make_shared<image_texture>("earthmap.jpg"));
    world.add(make_shared<sphere>(point3(400,200,400), 100, emat));
    auto pertext = make_shared<noise_texture>(0.2);
    world.add(make_shared<sphere>(point3(220,280,300), 80, make_shared<lambertian>(pertext)));

    hittable_list boxes2;
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(make_shared<sphere>(point3::random(0,165), 10, white));
    }

    world.add(make_shared<translate>(
        make_shared<rotate_y>(
            make_shared<bvh_node>(boxes2), 15),
            vec3(-100,270,395)
        )
    );

    camera cam;
    Visualizer visualizer(image_width, 1.0);

    cam.aspect_ratio = 1.0;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.background = color(0,0,0);

    cam.vfov = 40;
    cam.lookfrom = point3(478, 278, -600);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

void add_face(hittable_list& list, point3 a, point3 b, point3 c, const point3& inside, shared_ptr<material> mat) {
    if (dot(cross(b - a, c - a), a - inside) < 0)
        std::swap(b, c);
    list.add(make_shared<triangle>(a, b, c, mat));
}

hittable_list pyramid(const point3& base_center, double half, double height, shared_ptr<material> mat) {
    hittable_list tris;
    point3 p0 = base_center + vec3(-half, 0, -half);
    point3 p1 = base_center + vec3( half, 0, -half);
    point3 p2 = base_center + vec3( half, 0,  half);
    point3 p3 = base_center + vec3(-half, 0,  half);
    point3 apex = base_center + vec3(0, height, 0);
    point3 inside = base_center + vec3(0, height / 4, 0);

    add_face(tris, p0, p1, apex, inside, mat);
    add_face(tris, p1, p2, apex, inside, mat);
    add_face(tris, p2, p3, apex, inside, mat);
    add_face(tris, p3, p0, apex, inside, mat);
    add_face(tris, p0, p1, p2, inside, mat);
    add_face(tris, p0, p2, p3, inside, mat);
    return tris;
}

hittable_list tetrahedron(const point3& base_center, double r, shared_ptr<material> mat) {
    hittable_list tris;
    point3 v[3];
    for (int i = 0; i < 3; i++) {
        double phi = degrees_to_radians(90 + 120 * i);
        v[i] = base_center + vec3(r * std::cos(phi), 0, r * std::sin(phi));
    }
    point3 apex = base_center + vec3(0, r * std::sqrt(2.0), 0);
    point3 inside = base_center + vec3(0, r * std::sqrt(2.0) / 4, 0);

    add_face(tris, v[0], v[1], v[2], inside, mat);
    add_face(tris, v[0], v[1], apex, inside, mat);
    add_face(tris, v[1], v[2], apex, inside, mat);
    add_face(tris, v[2], v[0], apex, inside, mat);
    return tris;
}

hittable_list octahedron(const point3& center, double r, shared_ptr<material> mat) {
    hittable_list tris;
    for (int sx : {-1, 1})
        for (int sy : {-1, 1})
            for (int sz : {-1, 1})
                add_face(tris,
                         center + vec3(sx * r, 0, 0),
                         center + vec3(0, sy * r, 0),
                         center + vec3(0, 0, sz * r),
                         center, mat);
    return tris;
}

void triangles() {
    hittable_list world;

    auto ground = make_shared<lambertian>(make_shared<checker_texture>(0.5, color(.2, .2, .22), color(.8, .8, .8)));
    auto clay = make_shared<lambertian>(color(0.85, 0.45, 0.2));
    auto glass = make_shared<dielectric>(1.5);
    auto steel = make_shared<metal>(color(0.8, 0.85, 0.9), 0.05);
    auto light = make_shared<diffuse_light>(color(12, 12, 12));

    world.add(make_shared<triangle>(point3(-20, 0, -20), point3(20, 0, -20), point3(20, 0, 20), ground));
    world.add(make_shared<triangle>(point3(-20, 0, -20), point3(20, 0, 20), point3(-20, 0, 20), ground));

    world.add(make_shared<bvh_node>(pyramid(point3(-2.4, 0, 0), 0.9, 1.7, clay)));
    world.add(make_shared<bvh_node>(tetrahedron(point3(0, 0, 0.5), 1.0, glass)));
    world.add(make_shared<bvh_node>(octahedron(point3(2.4, 1.0, 0), 1.0, steel)));

    point3 la(-1.5, 5, -1), lb(1.5, 5, -1), lc(0, 5, 1.5);
    world.add(make_shared<triangle>(la, lb, lc, light));

    hittable_list lights;
    lights.add(make_shared<triangle>(la, lb, lc, shared_ptr<material>()));

    camera cam;
    Visualizer visualizer(1280, 16.0 / 9.0);

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = color(0.2, 0.25, 0.35);

    cam.vfov = 32;
    cam.lookfrom = point3(0, 3, 9);
    cam.lookat = point3(0, 0.9, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world, lights, [&](int y, const std::vector<color>& row) {
        visualizer.on_scanline(y, row);
    });
    visualizer.display();
}

int main(int argc, char* argv[]) {
    int scene = 0;
    if (argc > 1) {
        scene = std::atoi(argv[1]);
    }

    auto t_start = std::chrono::high_resolution_clock::now();

    switch(scene) {
        case 1: bounding_spheres(); break;
        case 2: checkered_spheres(); break;
        case 3: earth(); break;
        case 4: perlin_spheres(); break;
        case 5: quads(); break;
        case 6: simple_light(); break;
        case 7: cornell_box(); break;
        case 8: cornell_smoke(); break;
        case 9: final_scene(1000, 10000, 40); break;
        case 10: triangles(); break;
        default: final_scene(1000, 250, 4); break;
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(t_end - t_start);

    auto minutes = duration.count() / 60;
    auto seconds = duration.count() % 60;
    std::clog << "Czas renderowania: " << minutes << " min " << seconds << " sek\n";
}