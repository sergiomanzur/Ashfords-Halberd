#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>
using namespace ashford;
namespace {
void ck(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
template <class F> void bad(F fn) {
  bool failed = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Invalid standing-grab dependency accepted");
}
std::pair<ScenarioState, ScenarioEntity> fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t id : {401u, 402u, 404u, 405u, 426u, 427u, 429u, 430u, 451u,
                      452u, 454u, 455u}) {
    Motion m;
    m.frames = 60;
    bank->at(id) = m;
  }
  auto &e = s.enemies[0];
  e.id = 1;
  e.flags = 1;
  e.zombie_flags = 0x20000;
  e.zombie_work.emplace();
  e.motions = bank;
  e.angles[1] = 65530;
  e.frame = 11 * 65536;
  ScenarioEntity p;
  p.flags = 0x50015;
  p.modes = {4, 0, 1, 0};
  p.state_flags = 0x10001;
  p.attack_flags = 0xbeef;
  p.position = {1, 7, 2};
  p.angles[1] = 100;
  p.player_work.emplace();
  p.player_work->status_flags = 0xdead;
  p.player_work->movement_anchor = {1, 42, 2};
  auto base = std::make_shared<MotionBank>(64);
  Motion standing;
  standing.flags = 0x10;
  standing.frames = 2;
  standing.tracks.resize(1);
  standing.tracks[0].positions = {{20, 40, 60}, {20, 40, 60}};
  standing.tracks[0].angles = {{1000, 2000, 3000}, {1000, 2000, 3000}};
  standing.attributes = {0xabcd, 0xbeef};
  base->at(42) = standing;
  p.base_motions = base;
  p.motions = base;
  p.joint_pose.resize(1);
  p.joint_pose[0].position = Vec3{10, 20, 30};
  p.joint_pose[0].angles = std::array<int32_t, 3>{0, 0, 0};
  p.model_work.emplace();
  p.model_work->joints.resize(1);
  p.model_work->joints[0].matrix[3] = 12;
  p.model_work->joints[0].matrix[7] = std::numeric_limits<double>::quiet_NaN();
  p.model_work->joints[0].matrix[11] = -4;
  s.input_held = 0xabcd;
  return {s, p};
}
void starts() {
  constexpr uint32_t offsets[] = {400, 425, 450, 425};
  for (uint8_t character = 0; character < 4; ++character)
    for (bool back : {false, true}) {
      auto [s, p] = fixture();
      s.player_id = character;
      if (back)
        s.enemies[0].zombie_flags |= 0x4000;
      tick_zombie_player_standing_grab(s, 0, p);
      ck(p.modes[3] == 1 && p.flags == 0x11015 && p.control0 == (back ? 0 : 1),
         "Delayed standing startup flags/timer wrong");
      ck(p.motions == s.enemies[0].motions &&
             p.motion_number == offsets[character] + (back ? 1 : 2),
         "Standing character/facing clip wrong");
      ck(p.frame == 0 && p.blend_count == 5 && p.blend_rate == 42598 &&
             p.motion_step == 65536 && p.motion_mode == 0,
         "Standing initial motion counters wrong");
      ck(s.enemies[0].grab_yaw_offset == (back ? -32768 : 0) &&
             s.enemies[0].zombie_flags == (back ? 0xa4000u : 0xa0000u),
         "Standing attachment flags/yaw wrong");
      const auto a = s.enemies[0].zombie_work->action_anchor;
      ck(a.x == 0 && a.y == 0 &&
             std::fabs(a.z - (back ? -8.f : -8.8444757f)) < 1e-6f,
         "Standing attachment offset wrong");
      p.frame = 13 * 65536;
      s.enemies[0].frame = 19 * 65536;
      tick_zombie_player_standing_grab(s, 0, p);
      const int32_t expected = back ? 32762 : 65530;
      ck(p.yaw_correction == expected + (!back && character == 0 ? 7098 : 0),
         "Claire unwrapped frame546 yaw wrong");
      ck(s.pending_player_voices == std::vector<int32_t>{1026},
         "Standing frame19 voice missing");
      s.enemies[0].flags |= 0x10000;
      tick_zombie_player_standing_grab(s, 0, p);
      ck(s.pending_player_voices.size() == 1,
         "Suppressed standing voice queued");
      p.modes[3] = 2;
      tick_zombie_player_standing_grab(s, 0, p);
      ck(p.motion_number == offsets[character] + (back ? 4 : 5) &&
             p.modes[3] == 3 && p.frame == 0 && p.blend_count == 0 &&
             p.blend_rate == 32768,
         "Standing release setup/fallthrough wrong");
      ck(p.position.x == 1 && p.position.z == 2,
         "State2 incorrectly ran state3 cached relocation");
      ck(p.yaw_correction == (!back && character == 0 ? 32762 : expected),
         "Standing release orientation wrong");
      tick_zombie_player_standing_grab(s, 0, p);
      ck(p.position.x == 12 && p.position.y == 7 && p.position.z == -4,
         "Standing release consumed cached Y or missed XZ");
      ck(p.player_work->movement_anchor.x == 12 &&
             p.player_work->movement_anchor.y == 42 &&
             p.player_work->movement_anchor.z == -4 &&
             p.player_work->status_flags == 0xdead,
         "EXP0 movement anchor corrupted EXP1 or unused Y");
      ck(p.motions == p.base_motions && p.motion_number == 42 && p.frame == 0 &&
             p.motion_step == 0 && p.motion_mode == 0x100 &&
             p.blend_rate == 49152 && p.blend_count == 8,
         "Standing base42 immediate sample counters wrong");
      ck(p.flags == 0x19 && p.state_flags == 1 && p.attack_flags == 0 &&
             p.modes == std::array<uint8_t, 4>{1, 0, 0, 0} &&
             s.input_held == 0xabc0 && s.enemies[0].zombie_flags == 0,
         "Standing release cleanup flags/input wrong");
      ck(p.angles[1] == (!back && character == 0 ? 32868 : 100),
         "Standing release primary yaw flip wrong");
      auto pos = *p.joint_pose[0].position;
      ck(pos.x == 20 && pos.y == 40 && pos.z == 60 &&
             *p.joint_pose[0].angles == std::array<int32_t, 3>{250, 500, 750} &&
             p.motion_attribute == 0xabcd,
         "Immediate base42 SetMotion/Hokan sample missing");
    }
  for (bool back : {false, true}) {
    auto [s, p] = fixture();
    p.modes[3] = 4;
    p.control0 = 77;
    if (back)
      s.enemies[0].zombie_flags |= 0x4000;
    s.enemies[0].frame = 19 * 65536;
    tick_zombie_player_standing_grab(s, 0, p);
    ck(p.modes[3] == 5 && p.control0 == 0 &&
           p.angles[1] == (back ? 32762 : 65530),
       "Immediate standing startup facing/timer wrong");
    p.frame = 13 * 65536;
    tick_zombie_player_standing_grab(s, 0, p);
    ck(p.yaw_correction == (back ? 32762 : 65530),
       "State4 incorrectly incremented Claire front timer");
    ck(s.pending_player_voices == std::vector<int32_t>{1026, 1026},
       "State4->5 frame19 fallthrough voice wrong");
  }
}
void cache_layout_rollback() {
  int accepted = 0;
  for (int fault = 0; fault < 2; ++fault) {
    auto [s, p] = fixture();
    p.modes[3] = 3;
    p.frame = 0;
    s.enemies[0].zombie_flags = 0xa4080;
    if (fault == 0)
      p.joint_pose.push_back(p.joint_pose[0]);
    else
      p.model_work->joint_count = 2;
    const auto bank = p.motions;
    const auto scratch = s.motion_angle_scratch;
    bool rejected = false;
    try {
      tick_zombie_player_standing_grab(s, 0, p);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    if (!rejected) {
      ++accepted;
      std::cerr << (fault == 0 ? "Truncated cache layout"
                               : "Excessive selected cache range")
                << " committed standing release\n";
      continue;
    }
    ck(p.position.x == 1 && p.position.y == 7 && p.position.z == 2 &&
           p.angles[1] == 100 && p.flags == 0x50015 && p.modes[3] == 3 &&
           p.motions == bank && p.motion_number == 0 && p.blend_count == 0 &&
           p.frame == 0 && p.attack_flags == 0xbeef &&
           p.state_flags == 0x10001 && p.player_work->movement_anchor.x == 1 &&
           p.player_work->movement_anchor.z == 2 &&
           p.player_work->status_flags == 0xdead &&
           p.joint_pose[0].position->x == 10 &&
           (*p.joint_pose[0].angles)[0] == 0 &&
           s.enemies[0].zombie_flags == 0xa4080 && s.input_held == 0xabcd &&
           s.motion_angle_scratch == scratch,
       "Invalid cache-layout release leaked staged state");
  }
  ck(accepted == 0, "Standing release accepted invalid cached-work layout");
}
void rollback_and_gates() {
  auto [s, p] = fixture();
  p.modes[3] = 3;
  p.frame = 65536;
  p.model_work.reset();
  p.base_motions.reset();
  p.player_work.reset();
  tick_zombie_player_standing_grab(s, 0, p);
  ck(p.modes[3] == 3 && p.frame == 65536,
     "Nonzero release frame consumed missing cleanup context");
  std::tie(s, p) = fixture();
  p.modes[3] = 99;
  s.player_id = 255;
  p.motions.reset();
  p.player_work.reset();
  tick_zombie_player_standing_grab(s, 0, p);
  ck(p.modes[3] == 99, "Unknown standing submode consumed character/assets");
  for (int fault = 0; fault < 5; ++fault) {
    std::tie(s, p) = fixture();
    p.modes[3] = 3;
    p.frame = 0;
    s.enemies[0].zombie_flags = 0xa4080;
    if (fault == 0)
      p.base_motions.reset();
    if (fault == 1)
      p.model_work->joints.clear();
    if (fault == 2)
      p.player_work.reset();
    if (fault == 3)
      p.joint_pose[0].angles.reset();
    if (fault == 4)
      p.model_work->joints[0].matrix[3] = std::numeric_limits<double>::max();
    const auto bank = p.motions;
    const auto scratch = s.motion_angle_scratch;
    bad([&] { tick_zombie_player_standing_grab(s, 0, p); });
    ck(p.position.x == 1 && p.position.y == 7 && p.position.z == 2 &&
           p.angles[1] == 100 && p.flags == 0x50015 && p.modes[3] == 3 &&
           p.motions == bank && p.frame == 0 && p.motion_number == 0 &&
           p.blend_count == 0 && p.attack_flags == 0xbeef &&
           p.state_flags == 0x10001 && s.enemies[0].zombie_flags == 0xa4080 &&
           s.input_held == 0xabcd && s.pending_player_voices.empty() &&
           s.motion_angle_scratch == scratch,
       "Late standing release failure leaked staged writes");
  }
  std::tie(s, p) = fixture();
  p.modes[3] = 4;
  s.enemies[0].motions.reset();
  bad([&] { tick_zombie_player_standing_grab(s, 0, p); });
  ck(p.flags == 0x50015 && p.modes[3] == 4 &&
         s.enemies[0].zombie_flags == 0x20000,
     "Bad startup bank leaked flags/attachment");
  std::tie(s, p) = fixture();
  s.enemies[0].zombie_flags = 0;
  s.player_id = 255;
  tick_zombie_player_grab(s, 0, p);
  ck(p.modes[3] == 0, "Disabled standing gate ran callback");
  std::tie(s, p) = fixture();
  tick_zombie_player_grab(s, 0, p);
  ck(p.modes[3] == 1 && p.motion_number == 402,
     "PlayerControl failed to route mode2=1");
}
} // namespace
int main() {
  try {
    starts();
    rollback_and_gates();
    cache_layout_rollback();
    std::cout << "Native standing player grab checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
