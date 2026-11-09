#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "hittable.h"

class triangle : public hittable {
    public:
        triangle(const point3& a, const point3& b, const point3& c, shared_ptr<material> mat) : A(a), e1(b - a), e2(c - a), mat(mat) {
            auto n = cross(e1, e2);
            normal = unit_vector(n);
            area = 0.5 * n.length();

            bbox = aabb(aabb(a, b), aabb(a, c));
        }

        aabb bounding_box() const override { return bbox; }

        bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
            // metoda Mollera-Trumborego
            vec3 pvec = cross(r.direction(), e2);
            double det = dot(e1, pvec);

            if (std::fabs(det) < 1e-8)
                return false;

            double inv_det = 1.0 / det;
            vec3 tvec = r.origin() - A;

            double b1 = dot(tvec, pvec) * inv_det;
            if (b1 < 0 || b1 > 1)
                return false;
            
            vec3 qvec = cross(tvec, e1);
            double b2 = dot(r.direction(), qvec) * inv_det;
            if (b2 < 0 || b1 + b2 > 1)
                return false;

            double t = dot(e2, qvec) * inv_det;
            if (!ray_t.contains(t))
                return false;
            
            rec.t = t;
            rec.p = r.at(t);
            rec.u = b1;
            rec.v = b2;
            rec.mat = mat;
            rec.set_face_normal(r, normal);

            return true;
        }

        double pdf_value(const point3& origin, const vec3& direction) const override {
            hit_record rec;
            if (!this->hit(ray(origin, direction), interval(0.001, infinity), rec))
                return 0;

            auto dist_squared = rec.t * rec.t * direction.length_squared();
            auto cos = std::fabs(dot(direction, rec.normal) / direction.length());

            return dist_squared / (cos * area);
        }

        vec3 random(const point3& origin) const override {
            double s = std::sqrt(random_double());
            double r2 = random_double();
            point3 p = A + (s * (1 - r2)) * e1 + (s * r2) * e2;
            return p - origin;
        }

    private:
        point3 A;
        vec3 e1, e2;
        vec3 normal;
        double area;
        shared_ptr<material> mat;
        aabb bbox;
};

#endif