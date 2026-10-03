#include "zombie.hpp"
#include "zombie_motion_table_internal.hpp"
#include "zombie_motion_tables.hpp"
#include <cmath>
#include <iostream>
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
  ck(hit, "Invalid motion-table context not rejected");
}
bool near(float a, float b) { return std::fabs(a - b) < .0001f; }
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  auto &e = s.enemies[0];
  e.flags = 0x40001;
  e.id = 1;
  e.modes = {1, 0, 1, 1};
  e.position = {100, 5, 200};
  e.motion_number = 0;
  e.motion_mode = 0;
  e.frame = 999;
  e.control0 = 123;
  e.zombie_work.emplace();
  e.zombie_work->personality = 1;
  e.motions = std::make_shared<MotionBank>();
  e.joint_pose.resize(18);
  e.model_work.emplace();
  e.model_work->joints.resize(18);
  for (size_t i = 0; i < 18; ++i) {
    e.joint_pose[i].position = Vec3{float(i), 0, 0};
    e.joint_pose[i].angles = std::array<int32_t, 3>{};
    e.model_work->joints[i].matrix[3] = double(i * 2);
    e.model_work->joints[i].matrix[11] = -double(i);
  }
  return s;
}
int main() {
  try {
    auto s = fixture();
    tick_zombie_motion_table(s, 0, 0);
    auto &e = s.enemies[0];
    ck(near(e.position.x, -2) && e.position.y == 5 && near(e.position.z, -4) &&
           e.frame == 999 && e.control0 == 123 && s.random_seed == 1 &&
           s.pending_enemy_sounds.empty(),
       "Walking right foot correction/source frame or scalar preservation "
       "failed");
    s = fixture();
    s.enemies[0].motion_mode = 2;
    tick_zombie_motion_table(s, 0, 0);
    ck(near(s.enemies[0].position.x, -5) && near(s.enemies[0].position.z, -7),
       "Mirrored type8 foot swap failed");
    s = fixture();
    tick_zombie_motion_table(s, 0, 11);
    ck(near(s.enemies[0].position.x, -5) && near(s.enemies[0].position.z, -7),
       "Inclusive start/type7 foot failed");
    s = fixture();
    tick_zombie_motion_table(s, 0, 43);
    ck(near(s.enemies[0].position.x, -5), "Inclusive window end failed");
    s = fixture();
    tick_zombie_motion_table(s, 0, 44);
    ck(near(s.enemies[0].position.x, -2), "Next window boundary failed");
    s = fixture();
    tick_zombie_motion_table(s, 0, 60);
    ck(s.enemies[0].position.x == 100, "Outside window must not correct");
    for (bool left : {false, true}) {
      s = fixture();
      s.enemies[0].motion_number = 2;
      s.enemies[0].motion_mode = 2;
      if (left)
        s.enemies[0].flags |= 0x80000;
      tick_zombie_motion_table(s, 0, -123);
      ck(near(s.enemies[0].position.x, left ? -5 : -2),
         "Motion2 ignores table/frame and uses80000 foot");
    }
    s = fixture();
    s.enemies[0].motion_number = 125;
    tick_zombie_motion_table(s, 0, 0);
    ck(near(s.enemies[0].position.x, -22.4f) &&
           near(s.enemies[0].position.z, -44.8f),
       "Motion125 offset scale1.2 failed");
    s = fixture();
    s.enemies[0].joint_pose[4].angles = std::array<int32_t, 3>{0, 16384, 0};
    tick_zombie_motion_table(s, 0, 0);
    ck(near(s.enemies[0].position.x, -.1f) &&
           near(s.enemies[0].position.z, -5.9f),
       "Foot offset0/-1/-1.9 must be applied to both paths");
    s = fixture();
    s.enemies[0].motion_number = 49;
    tick_zombie_motion_table(s, 0, 0);
    ck(s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].code == 16847671 &&
           near(s.pending_enemy_sounds[0].position.x, -2) &&
           near(s.pending_enemy_sounds[0].position.z, -4),
       "Sound must use corrected actor position/personality");
    s = fixture();
    s.enemies[0].flags &= ~0x40000u;
    s.enemies[0].joint_pose.clear();
    s.enemies[0].state_flags = 0x100000;
    tick_zombie_motion_table(s, 0, 12);
    ck(s.enemies[0].position.x == 100 && s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].code == 70417,
       "Fix flag gate/special footstep sound remap failed");
    s = fixture();
    s.enemies[0].motion_number = 11;
    s.enemies[0].state_flags = 0x100000;
    tick_zombie_motion_table(s, 0, 16);
    ck(s.pending_enemy_sounds.empty(), "Special status must suppress8963");
    s = fixture();
    s.enemies[0].motion_number = 8;
    s.enemies[0].modes[0] = 4;
    tick_zombie_motion_table(s, 0, 24);
    ck(s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].code == 8960 &&
           s.pending_enemy_vibrations.size() == 1 &&
           s.pending_enemy_vibrations[0].work == 0 &&
           s.pending_enemy_vibrations[0].channel == 1 &&
           s.pending_enemy_vibrations[0].preset == 9,
       "Authored sound/vibration same-frame/preset failed");
    s = fixture();
    s.enemies[0].motion_number = 8;
    s.enemies[0].modes[0] = 5;
    tick_zombie_motion_table(s, 0, 24);
    ck(s.pending_enemy_vibrations.empty() && s.pending_enemy_sounds.size() == 1,
       "Mode5 must suppress only vibration");
    s = fixture();
    s.enemies[0].motion_number = 36;
    tick_zombie_motion_table(s, 0, 23);
    ck(s.pending_enemy_vibrations.size() == 1 &&
           s.pending_enemy_vibrations[0].preset == 10,
       "Vibration type1 must map preset10");
    s = fixture();
    s.enemies[0].motion_number = 7;
    tick_zombie_motion_table(s, 0, 17, {16786228});
    ck(s.random_seed == 1 && s.pending_enemy_sounds.empty() &&
           near(s.enemies[0].position.x, -5),
       "Resolved per-slot voice occupancy must short-circuit RNG without "
       "suppressing fix");
    tick_zombie_motion_table(s, 0, 17, {8963});
    ck(s.random_seed == 1 && s.pending_enemy_sounds.empty(),
       "Different active same-slot sound must also suppress the positive voice "
       "query");
    tick_zombie_motion_table(s, 0, 17);
    ck(s.random_seed == 1103527590 && s.pending_enemy_sounds.empty(),
       "Source even voice random suppression failed");
    tick_zombie_motion_table(s, 0, 17);
    ck(s.random_seed == 2524885223u && s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].code == 16786228,
       "Source odd voice request failed");
    s = fixture();
    s.room_motions = s.enemies[0].motions;
    s.enemies[0].pose_owner = 999;
    s.enemies[0].motion_number = 8;
    tick_zombie_motion_table(s, 0, 24);
    ck(s.pending_enemy_vibrations.empty() && s.pending_enemy_sounds.empty() &&
           s.enemies[0].position.x == 100,
       "Room-bank pointer gate must bypass all consumed context");
    s = fixture();
    s.enemies[0].motion_number = 999;
    s.enemies[0].pose_owner = 999;
    tick_zombie_motion_table(s, 0, 0);
    ck(s.enemies[0].position.x == 100,
       "Unknown motion must bypass unused owner");
    s = fixture();
    s.enemies[0].motion_number = 49;
    s.enemies[0].zombie_work->personality = 255;
    bad([&] { tick_zombie_motion_table(s, 0, 0); });
    ck(s.enemies[0].position.x == 100 && s.random_seed == 1 &&
           s.pending_enemy_sounds.empty(),
       "Invalid sound must roll back staged foot correction");
    s = fixture();
    s.enemies[0].joint_pose[4].position.reset();
    bad([&] { tick_zombie_motion_table(s, 0, 0); });
    ck(s.enemies[0].position.x == 100,
       "Invalid fix partially committed position");
    for (auto type : {0, 1, 2, 3, 6, 7, 8, 10, 11}) {
      s = fixture();
      auto fix = detail::evaluate_zombie_motion_fix(s, 0, type);
      const float x[14] = {-5, -2, -32, -29, 0,   0, 0,
                           -5, -2, 0,   -31, -28, 0, 0};
      ck(!fix.skip && fix.correction && fix.correction->actor == 0 &&
             near(fix.correction->position.x, x[type]),
         "Source extracted fix switch path failed");
    }
    s = fixture();
    s.enemies[0].zombie_work->left_leg_state = -1;
    auto skipped = detail::evaluate_zombie_motion_fix(s, 0, 10);
    ck(skipped.skip && !skipped.correction,
       "Missing left leg must continue window search");
    s = fixture();
    s.enemies[0].zombie_work->right_leg_state = -1;
    skipped = detail::evaluate_zombie_motion_fix(s, 0, 11);
    ck(skipped.skip && !skipped.correction,
       "Missing right leg must continue window search");
    s = fixture();
    auto &body = s.enemies[1];
    body.id = 1;
    body.flags = 0x81;
    body.pose_owner = 0;
    body.linked_parent = 0;
    body.zombie_work.emplace();
    for (auto type : {12, 13}) {
      auto parent = detail::evaluate_zombie_motion_fix(s, 1, type);
      ck(parent.correction && parent.correction->actor == 0 &&
             near(parent.correction->position.x, type == 12 ? -32 : -29),
         "Leg12/13 must correct parent root");
    }
    body.linked_parent = 999;
    body.zombie_work->left_leg_state = -1;
    ck(detail::evaluate_zombie_motion_fix(s, 1, 12).skip,
       "Negative leg must bypass unused parent");
    body.zombie_work->left_leg_state = 0;
    bad([&] { detail::evaluate_zombie_motion_fix(s, 1, 12); });
    std::cout << "Source motion-table foot windows, event requests, feedback "
                 "and rollback passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
