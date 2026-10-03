#include "scenario_motion.hpp"
#include "script_motion_fixture.hpp"
#include "zombie.hpp"
#include <cmath>
#include <iostream>
using namespace ashford;
using namespace script_fixture;
void selection() {
  auto s = fixture();
  const auto old = s;
  event(s, Bytes{101, 1, 0, 0, 105, 24, 1, 1, 4, 1, 0, 6, 55, 7, 0});
  const auto &r = s.enemies[2];
  const auto &b = s.enemies[3];
  ck(r.modes == std::array<uint8_t, 4>{5, 4, 0, 0} && r.motion_number == 4 &&
         r.motion_mode == 0x28 && r.frame == 0 && r.control1 == 0 &&
         r.control2 == 6 && r.motions == s.room_motions &&
         b.motion_number == 5 && b.modes[0] == 6 && b.frame == 0 &&
         b.blend_count == 0 && b.motions == s.room_motions &&
         b.pose_owner == 2 && r.base_motions == old.enemies[2].base_motions &&
         r.joint_pose[0].position->x == 2 && r.joint_pose[8].position->y == 1 &&
         s.room_case == 7 && s.tasks[0].pc == 14,
     "Original common24 must select root/body and sample root immediately");
  for (uint8_t immediate : {0, 1, 2, 3, 4, 5, 6, 7}) {
    auto v = fixture();
    v.enemies[2].joint_pose[0].position->x = 0;
    event(v, Bytes{101, 1, 0, 0, 105, 24, 1, 1, 4, 1, immediate, 6, 0});
    const bool sample =
        immediate == 0 || immediate == 2 || immediate == 5 || immediate == 6;
    ck(v.enemies[2].joint_pose[0].position->x == (sample ? 2 : 0) &&
           v.enemies[2].frame == 0 && v.enemies[3].frame == 0,
       "Immediate action must obey source sampling mask and add0");
  }
  auto blend = fixture();
  blend.enemies[2].blend_count = 2;
  blend.enemies[2].frame_direction = 0xffffffff;
  event(blend, Bytes{101, 1, 0, 0, 105, 24, 1, 1, 4, 0, 2, 6, 0});
  ck(blend.enemies[2].joint_pose[0].position->x == 2 &&
         blend.enemies[2].blend_count == 1 &&
         blend.enemies[2].motion_mode == 0x28 &&
         blend.enemies[2].motion_step == 65536 &&
         blend.enemies[2].frame_direction == 0xffffffff &&
         blend.enemies[3].frame_direction == 0,
     "Immediate add0/mode0 must override sampling arguments only");
  auto large = fixture();
  event(large, Bytes{101, 1, 0, 0, 105, 24, 4, 1, 4, 1, 1, 12, 0});
  ck(large.enemies[2].motion_step == 0x5555 &&
         large.enemies[2].control2 == 10 &&
         large.enemies[2].blend_count == 12 &&
         large.enemies[3].blend_count == 12 &&
         large.enemies[3].motion_mode == 0x28,
     "Source speed/blend table and companion count");
  auto invalid = fixture();
  invalid.enemies[2].zombie_work->body = 127;
  bad([&] {
    event(invalid, Bytes{101, 1, 0, 0, 105, 24, 1, 1, 4, 1, 0, 6, 0});
  });
  ck(invalid.enemies[2].motions == invalid.enemies[2].base_motions &&
         invalid.enemies[2].motion_number == 0,
     "Missing companion must retain original selection");
  invalid = fixture();
  invalid.enemies[3].linked_joint = 999;
  bad([&] {
    event(invalid, Bytes{101, 1, 0, 0, 105, 24, 1, 1, 4, 1, 0, 6, 0});
  });
  ck(invalid.enemies[2].motion_number == 0 &&
         invalid.enemies[2].joint_pose[0].position->x == 0,
     "Failing immediate linked calculation must retain pose/counters");
  for (const auto &tail :
       {Bytes{105, 24, 1, 1, 4, 1, 0}, Bytes{105, 24, 26, 1, 4, 1, 0, 6, 0},
        Bytes{105, 24, 1, 0, 4, 1, 0, 6, 0}}) {
    auto v = fixture();
    Bytes code{101, 1, 0, 0};
    code.insert(code.end(), tail.begin(), tail.end());
    bad([&] { event(v, code); });
    ck(v.enemies[2].motion_number == 0 && v.enemies[3].motion_number == 0,
       "Malformed instruction must retain root/body selection");
  }
  auto preserved = fixture();
  preserved.enemies[2].motion_number = 0x100;
  preserved.enemies[2].control0 = 123;
  preserved.enemies[2].control3 = 456;
  preserved.enemies[2].blend_count = 2;
  preserved.enemies[3].modes = {0, 12, 13, 14};
  select_scenario_motion(preserved, 1, 2, {1, 1, 4, 1, 1, 6});
  ck(preserved.enemies[2].motion_number == 0x104 &&
         preserved.enemies[2].control0 == 123 &&
         preserved.enemies[2].control3 == 456 &&
         preserved.enemies[2].blend_count == 2 &&
         preserved.enemies[3].modes == std::array<uint8_t, 4>{6, 12, 13, 14},
     "Source low-byte selection and unrelated root/body counters must survive");
  auto null_motion = preserved;
  select_scenario_motion(null_motion, 1, 2, {1, 1, 4, 1, 2, 6});
  ck(null_motion.enemies[2].blend_count == 2 &&
         null_motion.enemies[2].joint_pose[0].position->x == 0,
     "Null motion leaves blend counters and pose untouched");
  for (size_t size : {0u, 511u}) {
    auto invalid_bank = fixture();
    invalid_bank.room_motions = std::make_shared<MotionBank>(size);
    bad([&] {
      select_scenario_motion(invalid_bank, 1, 2, {1, 1, 4, 1, 0, 6});
    });
    ck(invalid_bank.enemies[2].motion_number == 0 &&
           invalid_bank.enemies[3].motion_number == 0,
       "Bad room bank rejects before root/body assignment");
  }
  auto stable = fixture();
  auto *stable_bound = &stable.enemies[2];
  select_scenario_motion(stable, 1, 2, {1, 1, 4, 0, 1, 16});
  ck(stable_bound == &stable.enemies[2],
     "Scenario selection must retain bound work references");
  auto deferred = fixture();
  deferred.enemies[2].motion_number = 0xab00;
  auto *bound = &deferred.enemies[2];
  select_scenario_motion(deferred, 1, 2, {1, 1, 7, 0, 1, 16});
  ck(bound == &deferred.enemies[2],
     "Scenario selection must retain bound work references");
  ck(bound->motion_number == 0xab07 && bound->blend_count == 16,
     "Non-sampling selection preserves source high bytes without bank lookup");
  bad([&] { select_scenario_motion(deferred, 1, 2, {1, 1, 7, 0, 2, 16}); });
  ck(bound == &deferred.enemies[2] && bound->motion_number == 0xab07,
     "Sampling must reject out-of-bank record without publishing staged work");
}
void events() {
  for (int index = 0; index < 15; ++index) {
    auto s = fixture();
    auto &r = s.enemies[2];
    r.motions = s.room_motions;
    r.modes = {5, 4, 0, 0};
    r.control2 = index;
    r.blend_count = 3;
    r.speed = 7;
    r.motion_mode = 0x20;
    tick_scripted_enemy(s, 2);
    const bool sampled =
        index == 4 || index == 5 || index == 8 || index == 9 || index >= 11;
    const int count = index == 4 || index == 6 || index == 7 ? 0
                      : index == 5                           ? 15
                      : index == 10                          ? 3
                      : sampled                              ? 7
                                                             : 8;
    ck(r.modes[3] == 1 && r.motion_number == 4 &&
           r.frame == (sampled ? 65536 : 0) && r.blend_count == count,
       "Original event table must retain callback sampling and blend counts");
    if (index == 5)
      ck((r.motion_mode & 0x100) && r.motion_step == 65536,
         "Source event6 sets ping-pong and normal step");
    if (index == 0 || index == 2 || index == 6 || index == 10)
      ck(r.speed == 0 && r.joint_pose[0].position->x == 0,
         "Static event callbacks must not sample or retain speed");
    for (uint8_t mode : {2, 3, 255}) {
      auto held = fixture();
      held.enemies[2].modes = {5, 4, 0, mode};
      held.enemies[2].control2 = index;
      tick_scripted_enemy(held, 2);
      ck(held.enemies[2].modes[3] == mode &&
             held.enemies[2].motion_number == 0 && held.enemies[2].frame == 0,
         "Source mode3 hold gate leaves fields/pose untouched");
    }
  }
  auto move = fixture();
  auto &m = move.enemies[2];
  m.motions = move.room_motions;
  m.modes = {5, 4, 0, 0};
  m.control2 = 7;
  m.control0 = 1000 << 16;
  m.control3 = 100;
  m.position = {};
  tick_scripted_enemy(move, 2);
  ck(m.angles[1] == -182 && m.position.x > 0 &&
         std::abs(m.position.z + 1) < .01f && m.frame == 0 &&
         m.blend_count == 0 && m.speed == 1,
     "Source move3_0 signed turn and movement with no callback sample");
  for (int index : {7, 9}) {
    auto signs = fixture();
    auto &r = signs.enemies[2];
    r.motions = signs.room_motions;
    r.modes = {5, 4, 0, 0};
    r.control2 = index;
    r.control0 = 0xff9c; // Source index7 signed -100; index9 unsigned65436.
    r.control3 = 100;
    r.position = {};
    tick_scripted_enemy(signs, 2);
    ck(r.angles[1] == (index == 7 ? 0 : -182),
       "Source event target low-half signedness must differ");
  }
  for (int index : {-1, 15}) {
    auto invalid = fixture();
    invalid.enemies[2].modes = {5, 4, 0, 0};
    invalid.enemies[2].control2 = index;
    bad([&] { tick_scripted_enemy(invalid, 2); });
    ck(invalid.enemies[2].modes[3] == 0 &&
           invalid.enemies[2].motion_number == 0,
       "Bad callback index must reject before counters change");
  }
  auto malformed = fixture();
  malformed.enemies[2].modes = {5, 4, 0, 0};
  malformed.enemies[2].control2 = 8;
  auto bank = std::make_shared<MotionBank>(*malformed.room_motions);
  (*bank)[4]->frames = 0;
  malformed.enemies[2].motions = bank;
  bad([&] { tick_scripted_enemy(malformed, 2); });
  ck(malformed.enemies[2].modes[3] == 0 &&
         malformed.enemies[2].motion_number == 0 &&
         malformed.enemies[2].joint_pose[0].position->x == 0,
     "Bad event motion must retain callback fields/pose");
}
void controllers() {
  ScenarioEntity player;
  player.flags = 1;
  player.player_work.emplace();
  player.position = {1000, 200, 1000};
  for (int control : {6, 8}) {
    auto s = fixture();
    s.collision = RoomCollision{};
    auto &r = s.enemies[2];
    r.motions = s.room_motions;
    r.modes = {5, 4, 0, 0};
    r.motion_number = 4;
    r.motion_mode = 0x28;
    r.control2 = control;
    tick_zombie_controller(s, 2, player);
    ck(r.frame == (control == 6 ? 65536 : 131072) && r.modes[3] == 1 &&
           r.blend_count == (control == 6 ? 0 : 6),
       "Zombie mode5 must run event before mandatory MainLoop SetMtn");
    auto &b = s.enemies[3];
    b.modes = {6, 123, 0, 0};
    b.control2 = 999; // Source body6 bypasses event table entirely.
    b.motion_number = 5;
    b.motion_mode = 0x28;
    b.motions = s.room_motions;
    tick_zombie_controller(s, 3, player);
    ck(b.frame == 65536 && b.modes[3] == 0 && r.joint_pose[8].position->y == 4,
       "Body6 bypass must still sample its own shared upper channels once");
  }
  for (uint8_t mode : {5, 6}) {
    auto s = fixture();
    s.enemies[3].flags = 0;
    s.flags[6][0] = 2;
    auto &r = s.enemies[2];
    r.id = 67;
    r.motions = s.room_motions;
    r.modes = {mode, 4, 0, 0};
    r.control2 = 4;
    r.damage[0] = 99;
    tick_scenario_actors(s);
    ck(r.frame == 65536 && r.modes[3] == 1 && r.rendered_model &&
           (r.flags & 0x8100) == 0x8100 && r.damage[0] == 0,
       "Generic subpl5/6 callback samples once then calculates and cleans "
       "damage");
  }
}
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "events")
      events();
    else if (argc == 2 && std::string(argv[1]) == "controllers")
      controllers();
    else
      selection();
    std::cout << "Scripted zombie motion checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
