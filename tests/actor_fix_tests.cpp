#include "actor.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
template <class F> void bad(F f) {
  bool hit = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    hit = true;
  }
  ck(hit, "Undefined fix context did not reject");
}
bool near(float a, float b) { return std::fabs(a - b) < .0001f; }
void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
ActorModel model() {
  ActorModel a;
  auto &b = a.model;
  b.resize(16 + 52 * 3);
  b[0] = 'M';
  b[1] = 'D';
  b[2] = 'L';
  b[3] = 128;
  b[4] = 16;
  b[6] = 3;
  put(b, 8, 0xffffffff);
  for (size_t i = 0; i < 3; ++i) {
    auto o = 16 + i * 52;
    put(b, o + 4, 0xffffffff);
    put(b, o + 44, i == 2 ? 0xffffffff : uint32_t((i + 1) * 52));
    put(b, o + 48, 0xffffffff);
  }
  return a;
}
int main() {
  try {
    std::vector<JointPose> pose(3);
    for (size_t i = 0; i < 3; ++i) {
      pose[i].position = Vec3{float(i + 1), 0, 0};
      pose[i].angles = std::array<int32_t, 3>{};
    }
    ActorModelWork cached;
    cached.joints.resize(3);
    cached.joints[2].matrix[3] = 7;
    cached.joints[2].matrix[11] = 8;
    ActorPlacement p;
    p.position = {10, 5, 20};
    std::vector<int8_t> path{0, 1, 2, -1, 100};
    auto out = calculate_actor_fix_offset(pose, cached, p, path);
    ck(near(out.x, 9) && near(out.y, 5) && near(out.z, 12),
       "Current path minus terminal cache failed");
    cached.first_joint = 99;
    cached.joint_count = 0;
    cached.joints[2].flags = 1;
    cached.base_matrix[0] = std::numeric_limits<double>::quiet_NaN();
    out = calculate_actor_fix_offset(pose, cached, p, path);
    ck(near(out.x, 9) && near(out.z, 12),
       "Fix must ignore cache flags/ranges/base and path padding");
    p.angles[1] = -49152;
    out = calculate_actor_fix_offset(pose, cached, p, path);
    ck(near(out.x, 3) && near(out.y, 5) && near(out.z, 6),
       "Signed word yaw transform failed");
    p.flags = 0x1000;
    out = calculate_actor_fix_offset(pose, cached, p, path);
    ck(near(out.x, 3) && near(out.z, 6),
       "Fix must ignore primary1000 base retention");
    p.flags = 0x1080;
    p.angles = {};
    p.position.x = std::numeric_limits<float>::quiet_NaN();
    p.linked_offset = {1, 2, 3};
    p.parent_joint =
        ModelMatrix{0, 0, 1, 100, 0, 1, 0, 50, -1, 0, 0, 200, 0, 0, 0, 1};
    out = calculate_actor_fix_offset(pose, cached, p, path);
    ck(near(out.x, 96) && near(out.y, 52) && near(out.z, 185),
       "Linked parent/local path and1000-ignore failed");
    p = ActorPlacement{};
    std::vector<JointPose> single(1);
    single[0].position = Vec3{};
    single[0].angles = std::array<int32_t, 3>{0, 16384, 0};
    ActorModelWork unit;
    unit.joints.resize(1);
    out = calculate_actor_fix_offset(single, unit, p, {0, -1}, {0, -1, -1.9f});
    ck(near(out.x, -1.9f) && near(out.y, 0) && near(out.z, 1.9f),
       "Current and cached offset points must both transform");
    bad([&] { calculate_actor_fix_offset(pose, cached, p, {}); });
    bad([&] { calculate_actor_fix_offset(pose, cached, p, {-1}); });
    bad([&] { calculate_actor_fix_offset(pose, cached, p, {0, 1}); });
    bad([&] { calculate_actor_fix_offset(pose, cached, p, {3, -1}); });
    bad([&] { calculate_actor_fix_offset(pose, cached, p, {-2, -1}); });
    auto missing = pose;
    missing[1].position.reset();
    bad([&] { calculate_actor_fix_offset(missing, cached, p, path); });
    auto short_cache = cached;
    short_cache.joints.resize(2);
    bad([&] { calculate_actor_fix_offset(pose, short_cache, p, path); });
    p.flags = 0x80;
    bad([&] { calculate_actor_fix_offset(pose, cached, p, path); });
    auto actor = model();
    p = ActorPlacement{};
    p.position = {3.141f, -1.235f, 123.5f};
    p.angles = {1234, -14563, 2468};
    pose[0].angles = std::array<int32_t, 3>{5678, 9102, -2000};
    pose[1].angles = std::array<int32_t, 3>{-9876, 1111, 3333};
    auto calculated = calculate_actor_model_work(actor, pose, {}, p);
    for (Vec3 offset : {Vec3{}, Vec3{0, -1, -1.9f}}) {
      out = calculate_actor_fix_offset(pose, calculated, p, path, offset);
      ck(out.x == 0 && out.y == 0 && out.z == 0,
         "Unchanged native cache/pose must have exactly zero drift");
    }
    ck(pose[0].position->x == 1 && calculated.joints.size() == 3,
       "Fix changed caller pose/cache");
    std::cout << "Source foot-fix path, prior matrix, linked placement and "
                 "zero-drift cache passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
