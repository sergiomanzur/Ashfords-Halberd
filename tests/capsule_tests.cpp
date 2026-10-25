#include "capsule.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>

using namespace ashford;

namespace {
void ck(bool v, const char* m) {
    if (!v) throw std::runtime_error(m);
}

void test_point_line() {
    Line3 line{{0, 0, 0}, {1, 0, 0}};
    Vec3 closest;
    float dist = distance_point_line({0, 5, 0}, line, &closest);
    ck(std::fabs(dist - 5.0f) < 1e-4f, "Point line distance should be 5");
    ck(std::fabs(closest.x) < 1e-4f && std::fabs(closest.y) < 1e-4f && std::fabs(closest.z) < 1e-4f, "Closest should be origin");

    dist = distance_point_line({3, 4, 0}, line, &closest);
    ck(std::fabs(dist - 4.0f) < 1e-4f, "Point line distance should be 4");
    ck(std::fabs(closest.x - 3.0f) < 1e-4f && std::fabs(closest.y) < 1e-4f, "Closest x should be 3");
}

void test_line_line() {
    Line3 a{{0, 0, 0}, {1, 0, 0}};
    Line3 b{{0, 5, 0}, {0, 0, 1}};
    Vec3 ca, cb;
    float dist = distance_line_line(a, b, ca, cb);
    ck(std::fabs(dist - 5.0f) < 1e-4f, "Skew line distance should be 5");
    ck(std::fabs(ca.x) < 1e-4f && std::fabs(ca.y) < 1e-4f && std::fabs(ca.z) < 1e-4f, "Closest a origin");
    ck(std::fabs(cb.x) < 1e-4f && std::fabs(cb.y - 5.0f) < 1e-4f && std::fabs(cb.z) < 1e-4f, "Closest b (0,5,0)");
}

void test_spheres() {
    Sphere3 s1{{0, 0, 0}, 2.0f};
    Sphere3 s2{{3, 0, 0}, 1.5f};
    ck(spheres_collide(s1, s2), "Spheres overlap (dist 3 <= 3.5)");
    Sphere3 s3{{5, 0, 0}, 1.0f};
    ck(!spheres_collide(s1, s3), "Spheres do not overlap (dist 5 > 3)");
}

void test_capsules_collide() {
    Capsule3 c1{{0, 0, 0}, {10, 0, 0}, 1.0f};
    Capsule3 c2{{5, 1.5f, 0}, {5, 5, 0}, 1.0f};
    Vec3 hit;
    ck(collide_capsules_ex(c1, c2, hit), "Capsules should collide (dist 1.5 <= 2.0)");
    
    Capsule3 c3{{5, 3.0f, 0}, {5, 6, 0}, 0.5f};
    ck(!collide_capsules_ex(c1, c3, hit), "Capsules should not collide (dist 3.0 > 1.5)");
}

void test_point_capsule() {
    Capsule3 cap{{0, 0, 0}, {10, 0, 0}, 2.0f};
    Vec3 p1{5, 0, 0}; // inside
    Vec3 res = distance_point_capsule(p1, cap);
    ck(std::fabs(res.x - 5.0f) < 1e-4f && std::fabs(res.y) < 1e-4f, "Point inside capsule stays inside");

    Vec3 p2{5, 5, 0}; // outside
    res = distance_point_capsule(p2, cap);
    ck(std::fabs(res.x - 5.0f) < 1e-4f && std::fabs(res.y - 2.0f) < 1e-4f, "Point outside projects to surface");
}

} // namespace

int main() {
    try {
        test_point_line();
        test_line_line();
        test_spheres();
        test_capsules_collide();
        test_point_capsule();
        std::cout << "Capsule tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Capsule test failed: " << e.what() << "\n";
        return 1;
    }
}
