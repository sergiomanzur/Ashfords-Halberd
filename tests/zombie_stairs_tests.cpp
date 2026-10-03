#include "enemy_navigation.hpp"
#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>
using namespace ashford;
namespace {
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Malformed consumed stair context did not reject");
}
std::pair<ScenarioState, ScenarioEntity> fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.collision.emplace();
  s.collision->ground[1] = -8;
  s.collision->ground[2] = 4;
  s.collision->ground[3] = 16;
  s.collision->ground[31] = 999; // Source floor lookup excludes final slot31.
  CollisionRecord a;
  a.flags = 1;
  a.type = 1;
  a.attribute = 0x10;
  a.position = {NAN, NAN, NAN};
  a.size = {NAN, NAN, NAN};
  a.parameters = {0, 2, 6, 1};
  auto b = a;
  b.parameters = {1, 0, 6, 0};
  s.collision->miscellaneous = {a, b};
  auto &e = s.enemies[0];
  e.flags = 0x41;
  e.id = 1;
  e.zombie_work.emplace();
  e.zombie_work->body = 1;
  e.zombie_work->stair_target = 0;
  e.position = {2, 4, 6};
  e.previous_position = {8, 9, 10};
  e.angles[1] = 5000;
  e.zombie_flags = 0x400001;
  e.modes = {1, 0, 8, 0};
  e.zombie_walk_choice = 0;
  e.model_work.emplace();
  e.model_work->joints.resize(1);
  e.model_work->joints[0].matrix[7] = 100;
  e.joint_pose.resize(1);
  e.joint_pose[0].position = Vec3{5, 9, 7};
  auto &body = s.enemies[1];
  body.flags = 0x81;
  body.id = 1;
  body.zombie_work.emplace();
  body.zombie_flags = 0x80000001;
  body.modes = {1, 9, 8, 7};
  body.frame = 888;
  reserve_actor_stairs(s, 0, 0);
  ScenarioEntity p;
  p.position = {8, NAN, 9};
  return {s, p};
}
void unchanged(const ScenarioState &s) {
  const auto &e = s.enemies[0];
  ck(e.position.y == 4 && e.previous_position.y == 9 && e.floor == 0 &&
         e.joint_pose[0].position->y == 9 && e.angles[1] == 5000 &&
         e.motion_number == 0 && s.enemies[1].motion_number == 0 &&
         s.collision->miscellaneous[0].attribute == 0x400010 &&
         s.random_seed == 1,
     "Stair validation failure partially committed state");
}
} // namespace
int main() {
  try {
    auto [s, p] = fixture();
    tick_zombie_stairs(s, 0, p);
    auto &e = s.enemies[0];
    auto &b = s.enemies[1];
    ck(e.motion_number == 2 && b.motion_number == 202 && e.control0 == 9 &&
           e.angles[1] == 7776 && e.floor_target_height == 16 &&
           e.pitch_correction == 32768 && e.yaw_correction == 2776 &&
           e.zombie_flags == 0xc00000 && e.zombie_secondary_flags == 8 &&
           e.secondary_flags == 1 && e.turn_speed == 256 && e.control1 == 4 &&
           e.modes[3] == 1 && e.blend_count == 10 &&
           e.zombie_work->stair_start_floor == 0 &&
           e.zombie_work->stair_step == 0 && e.zombie_work->target_x == 8 &&
           e.zombie_work->target_z == 9 &&
           e.zombie_work->action_anchor.y == 4 && b.modes[3] == 7 &&
           s.random_seed == 1,
       "MV08 source startup/fallthrough mismatch");
    for (int direction = 0; direction < 4; ++direction) {
      auto [x, q] = fixture();
      x.collision->miscellaneous[0].parameters[1] = uint8_t(direction);
      tick_zombie_stairs(x, 0, q);
      ck(x.enemies[0].pitch_correction ==
             std::array<int32_t, 4>{0, 49152, 32768, 16384}[direction],
         "Source stair yaw table mismatch");
    }
    for (int delta : {32768, 32769}) {
      auto [x, q] = fixture();
      x.enemies[0].angles[1] = 32768 - delta;
      tick_zombie_stairs(x, 0, q);
      ck(x.enemies[0].yaw_correction == (delta == 32768 ? 3276 : -3276),
         "Stair half-turn threshold mismatch");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.zombie_work->stair_target = 1;
      x.collision->miscellaneous[1].parameters[2] = 255;
      tick_zombie_stairs(x, 0, q);
      ck(w.floor_target_height == -506 && w.zombie_work->stair_step == -1 &&
             w.control1 == 253 && !(w.zombie_flags & 0x800000),
         "Unsigned prm2/signed stair byte distinction lost");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.motion_number = 2;
      w.blend_count = 0;
      tick_zombie_stairs(x, 0, q);
      ck(w.motion_number == 58 && x.enemies[1].motion_number == 258 &&
             w.modes[3] == 2 && w.angles[1] == 32768 && w.blend_count == 10 &&
             w.zombie_flags == 0x1c00000,
         "Same idle motion must immediately enter paired stairs when blend is "
         "zero");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 1;
      w.blend_count = 2;
      w.control0 = std::numeric_limits<int32_t>::min();
      w.yaw_correction = 3;
      w.zombie_work->body = 127;
      tick_zombie_stairs(x, 0, q);
      ck(w.control0 == std::numeric_limits<int32_t>::max() &&
             w.angles[1] == 5003 && w.modes[3] == 1,
         "Source stair timer word wrap/unused body changed");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 1;
      w.blend_count = 2;
      w.control0 = 1;
      w.yaw_correction = 1;
      w.angles[1] = std::numeric_limits<int32_t>::max();
      tick_zombie_stairs(x, 0, q);
      ck(w.angles[1] == std::numeric_limits<int32_t>::min(),
         "Stair yaw addition must wrap the source32bit word");
    }
    for (bool up : {false, true}) {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 2;
      w.flags |= 0x2000000;
      w.zombie_flags = up ? 0x800000 : 0;
      w.zombie_work->stair_step = up ? 127 : -128;
      tick_zombie_stairs(x, 0, q);
      ck(w.motion_number == (up ? 56 : 57) &&
             x.enemies[1].motion_number == (up ? 256 : 257) &&
             w.frame == 65536 && w.modes[3] == 3 && w.blend_count == 0 &&
             w.position.y == (up ? 101.4939f : 100.8804f) &&
             w.zombie_work->stair_step == (up ? -128 : 127) &&
             !(w.flags & 0x2000000),
         "Stair entry cachedY/byte wrap/paired motion mismatch");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 3;
      w.frame = 40 * 65536 + 123;
      w.zombie_flags = 0x800000;
      w.zombie_work->body = 127;
      q.position = {NAN, NAN, NAN};
      tick_zombie_stairs(x, 0, q);
      tick_zombie_stairs(x, 0, q);
      ck(w.zombie_work->stair_step == 2,
         "Repeated authored frame40 must increment each source tick");
    }
    for (int frame : {-1, 39, 40, 41}) {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 3;
      w.frame = frame * 65536 + 123;
      w.zombie_flags = 0x800000;
      w.zombie_work->stair_step = 3;
      tick_zombie_stairs(x, 0, q);
      ck(w.zombie_work->stair_step == (frame == 40 ? 4 : 3),
         "Stair counter must consume only exact authored frame40");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      auto &owner = x.enemies[3];
      owner.flags = 1;
      owner.model_work = w.model_work;
      owner.model_work->joints[0].matrix[7] = 200;
      w.pose_owner = 3;
      w.modes[3] = 2;
      w.flags |= 0x2000000;
      w.zombie_flags = 0x800000;
      tick_zombie_stairs(x, 0, q);
      ck(w.position.y == 201.4939f && w.model_work->joints[0].matrix[7] == 100,
         "Stair cachedY must come from actual model owner");
    }
    for (int remaining : {4, 2, 3}) {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 3;
      w.motion_number = 56;
      w.flags |= 0x2000000;
      w.zombie_flags = 0x800000;
      w.control1 = remaining;
      tick_zombie_stairs(x, 0, q);
      ck(w.control1 == remaining - 2 && w.frame == 65536 &&
             w.zombie_work->stair_step == 1 &&
             w.modes[3] == (remaining == 2 ? 4 : 3) &&
             w.motion_number == (remaining == 2 ? 60 : 56) &&
             x.enemies[1].frame == (remaining == 2 ? 65536 : 888),
         "Stair repeat requires exact zero and resets root frame only");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 4;
      w.motion_number = 60;
      w.flags |= 0x2000000;
      w.zombie_flags = 0xc00008;
      w.floor_target_height = 16;
      w.secondary_flags = 5;
      w.zombie_secondary_flags = 0x19;
      tick_zombie_stairs(x, 0, q);
      ck(w.floor == 1 && w.position.y == 16 && w.previous_position.y == 4 &&
             w.zombie_work->action_anchor.y == 4 &&
             w.joint_pose[0].position->y == 0 &&
             w.joint_pose[0].position->x == 5 &&
             w.joint_pose[0].position->z == 7 && w.motion_number == 0 &&
             x.enemies[1].motion_number == 200 && w.frame == 3276800 &&
             w.blend_count == 15 && w.flags == 0x40041 &&
             w.secondary_flags == 4 && w.zombie_flags == 9 &&
             w.zombie_secondary_flags == 0x11 &&
             w.modes == std::array<uint8_t, 4>{1, 1, 1, 0} &&
             x.enemies[1].modes == std::array<uint8_t, 4>{1, 9, 8, 7} &&
             w.stair_index == 0 && w.zombie_work->stair_target == 0 &&
             x.collision->miscellaneous[0].attribute == 0x10 &&
             x.collision->miscellaneous[1].attribute == 0x10,
         "Stair final floor/previous position/pose/walking/reservation handoff "
         "mismatch");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      auto &owner = x.enemies[3];
      owner.flags = 1;
      owner.joint_pose.resize(1);
      owner.joint_pose[0].position = Vec3{3, 12, 4};
      w.pose_owner = 3;
      w.modes[3] = 4;
      w.flags |= 0x2000000;
      w.floor_target_height = 16;
      tick_zombie_stairs(x, 0, q);
      ck(owner.joint_pose[0].position->y == 0 &&
             w.joint_pose[0].position->y == 9,
         "Completion must reset actual model owner pose");
    }
    {
      auto [x, q] = fixture();
      auto a = x.collision->miscellaneous[0];
      a.parameters[3] = 0;
      x.dynamic_miscellaneous = {a};
      x.enemies[0].stair_index = 2;
      auto &w = x.enemies[0];
      w.modes[3] = 4;
      w.flags |= 0x2000000;
      w.floor_target_height = 16;
      tick_zombie_stairs(x, 0, q);
      ck(!(x.dynamic_miscellaneous[0].attribute & 0x400000) &&
             !(x.collision->miscellaneous[0].attribute & 0x400000) &&
             (x.collision->miscellaneous[1].attribute & 0x400000),
         "Release must use stored dynamic kdnp pair, not the traversal target");
    }
    {
      auto [x, q] = fixture();
      x.dynamic_miscellaneous = {x.collision->miscellaneous[1]};
      x.dynamic_miscellaneous[0].flags = 0;
      x.enemies[0].zombie_work->stair_target = 2;
      tick_zombie_stairs(x, 0, q);
      ck(x.enemies[0].floor_target_height == -8 &&
             x.enemies[0].zombie_work->stair_step == 6,
         "Stored inactive dynamic stair remains readable");
    }
    for (int failure = 0; failure < 5; ++failure) {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 4;
      w.flags |= 0x2000000;
      w.floor_target_height = 16;
      if (failure == 0)
        w.stair_index = 128;
      if (failure == 1)
        w.zombie_walk_choice.reset();
      if (failure == 2)
        w.pose_owner = 127;
      if (failure == 3)
        w.floor_target_height = NAN;
      if (failure == 4)
        x.collision->ground[3] = -INFINITY;
      bad([&] { tick_zombie_stairs(x, 0, q); });
      unchanged(x);
    }
    {
      auto [x, q] = fixture();
      x.enemies[0].modes[3] = 2;
      x.enemies[0].flags |= 0x2000000;
      x.enemies[0].zombie_work->body = 127;
      bad([&] { tick_zombie_stairs(x, 0, q); });
      unchanged(x);
    }
    {
      auto [x, q] = fixture();
      x.enemies[0].modes[3] = 2;
      x.enemies[0].flags |= 0x2000000;
      x.enemies[0].model_work->joints[0].matrix[7] = 1e300;
      bad([&] { tick_zombie_stairs(x, 0, q); });
      unchanged(x);
    }
    for (int submode : {2, 4, 5, 9}) {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = uint8_t(submode);
      w.model_work.reset();
      w.zombie_work->body = 127;
      w.zombie_work->stair_target = 999;
      x.collision.reset();
      q.position = {NAN, NAN, NAN};
      tick_zombie_stairs(x, 0, q);
      ck(w.modes[3] == submode && w.motion_number == 0,
         "Unused stair branch consumed context");
    }
    {
      auto [x, q] = fixture();
      auto &w = x.enemies[0];
      w.modes[3] = 5;
      w.flags |= 0x2000000;
      w.secondary_flags = 5;
      w.zombie_work->body = 127;
      x.collision.reset();
      q.position = {NAN, NAN, NAN};
      tick_zombie_stairs(x, 0, q);
      ck(w.modes == std::array<uint8_t, 4>{1, 1, 1, 0} &&
             w.secondary_flags == 4,
         "Complete alternate stair state5 handoff failed");
    }
    std::cout << "Native zombie stair traversal checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
