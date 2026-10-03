#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
using namespace ashford;
namespace {
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void bad(F fn) {
  bool failed = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Invalid consumed attack context did not reject");
}
std::pair<ScenarioState, ScenarioEntity> fixture() {
  ScenarioState state;
  state.enemies.resize(128);
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t motion : {122u, 322u, 121u, 321u, 85u, 285u}) {
    Motion m;
    m.frames = 24;
    bank->at(motion) = m;
  }
  auto &root = state.enemies[0];
  root.flags = 0x41;
  root.id = 1;
  root.modes = {1, 0, 5, 0};
  root.zombie_work.emplace();
  root.zombie_work->body = 1;
  root.zombie_work->personality = 1;
  root.zombie_flags = 0x3000000;
  root.motions = bank;
  root.model_work.emplace();
  root.model_work->joints.resize(18);
  for (auto &joint : root.model_work->joints) {
    joint.matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  }
  auto &body = state.enemies[1];
  body.flags = 0x81;
  body.id = 1;
  body.zombie_work.emplace();
  body.zombie_flags = 0x83000000;
  body.motions = bank;
  ScenarioEntity player;
  player.flags = 1;
  player.health = 100;
  player.position = {0, 0, -4};
  player.angles[1] = 32768;
  player.model_work = root.model_work;
  for (auto &joint : player.model_work->joints)
    joint.matrix[11] = -4;
  player.geometry.capsule_start = {0, -1, -4};
  player.geometry.capsule_end = {0, 1, -4};
  player.geometry.capsule_radius = 1;
  return {state, player};
}
} // namespace
int main(int argc, char **argv) {
  try {
    auto [s, p] = fixture();
    if (argc == 2 && std::string(argv[1]) == "--capture") {
      ck(zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
         "Native cached joint capture cone must admit nearby player");
      p.model_work->joints[3].matrix[7] = NAN;
      s.enemies[0].model_work->joints[10].matrix[7] = NAN;
      ck(zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
         "Capture planar distance must ignore cached joint Y");
      std::tie(s, p) = fixture();
      p.model_work->joints[3].matrix[11] = -5.5f;
      ck(!zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
         "Capture distance boundary is strict");
      p.model_work->joints[3].matrix[11] = -5.49f;
      ck(zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
         "Capture just inside distance rejected");
      p.position.y = .01f;
      ck(!zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
         "Capture requires exact Y equality");
      p.position.y = 0;
      p.health = -1;
      ck(!zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
         "Dead player admitted capture");
      p.health = 0;
      ck(zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
         "Zero health admits source capture");
      std::tie(s, p) = fixture();
      p.position.z = 4;
      ck(!zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
         "Cone uses player actor position");
      ck(zombie_capture_check(s, 0, p, 0, NAN, 1),
         "Mode1 ignores cone/distance");
      p.model_work.reset();
      s.enemies[0].zombie_flags |= 0x20000000;
      ck(!zombie_capture_check(s, 0, p, 0, NAN, 2),
         "Suppression ignores missing cache");
      std::tie(s, p) = fixture();
      for (uint32_t flags : {2u, 0x80000000u}) {
        p.flags = flags == 2 ? 2 : 1;
        p.state_flags = flags == 2 ? 0 : flags;
        ck(!zombie_capture_check(s, 0, p, 3640, 5.5f, 0),
           "Player suppression flags ignored");
      }
      std::tie(s, p) = fixture();
      ck(zombie_capture_check(s, 0, p, 0, NAN, 2),
         "Transformed sphere/capsule hit missing");
      p.geometry.capsule_start.x = p.geometry.capsule_end.x = 2;
      ck(zombie_capture_check(s, 0, p, 0, NAN, 2),
         "Sphere/capsule tangent is inclusive");
      p.geometry.capsule_start.x = p.geometry.capsule_end.x = 2.01f;
      ck(!zombie_capture_check(s, 0, p, 0, NAN, 2), "Outside tangent admitted");
      for (float direction : {-1.f, 1.f}) {
        p.geometry.capsule_start = {0, direction > 0 ? 2.f : -4.f, -4};
        p.geometry.capsule_end = {0, direction > 0 ? 4.f : -2.f, -4};
        ck(zombie_capture_check(s, 0, p, 0, NAN, 2),
           "Capsule endpoint tangent rejected");
        p.geometry.capsule_start.y += direction * .01f;
        p.geometry.capsule_end.y += direction * .01f;
        ck(!zombie_capture_check(s, 0, p, 0, NAN, 2),
           "Endpoint outside tangent admitted");
      }
      p.geometry.capsule_start = p.geometry.capsule_end = {0, 0, -4};
      ck(zombie_capture_check(s, 0, p, 0, NAN, 2),
         "Degenerate capsule endpoint missing");
      p.geometry.capsule_radius = -1;
      bad([&] { zombie_capture_check(s, 0, p, 0, NAN, 2); });
    } else {
      tick_zombie_lunge(s, 0, p);
      ck(s.enemies[0].motion_number == 122 &&
             s.enemies[1].motion_number == 322 && s.enemies[0].modes[3] == 1 &&
             s.pending_enemy_sounds.size() == 1,
         "Native lunge startup/fallthrough failed");
      ck(s.enemies[0].zombie_flags == 0x1000000 &&
             s.enemies[1].zombie_flags == 0x81000000 &&
             s.enemies[0].blend_count == 8 && s.enemies[1].blend_count == 8 &&
             s.pending_enemy_sounds[0].code == 16782134 && s.random_seed == 1,
         "Startup flags/voice state mismatch");
      for (uint8_t character :
           {uint8_t(0), uint8_t(1), uint8_t(3), uint8_t(4)}) {
        std::tie(s, p) = fixture();
        s.player_id = character;
        s.enemies[0].modes[3] = 1;
        s.enemies[0].frame = 8 * 65536;
        tick_zombie_lunge(s, 0, p);
        const uint32_t main = character == 1 || character == 3 ? 121 : 85;
        ck(s.enemies[0].motion_number == main &&
               s.enemies[1].motion_number == main + 200 &&
               s.enemies[0].modes == std::array<uint8_t, 4>{2, 0, 0, 2} &&
               s.enemies[1].modes == s.enemies[0].modes &&
               p.modes == std::array<uint8_t, 4>{4, 0, 0, 4} &&
               p.flags == 0x10005 && s.enemies[0].control0 == 75 &&
               (s.enemies[0].zombie_flags & 0x24085) == 0x24085 &&
               !(s.enemies[0].flags & 0x40) && s.enemies[0].blend_count == 5,
           "Capture character pair/player packed state mismatch");
      }
      for (int32_t frame : {7, 8, 11, 12, 13}) {
        std::tie(s, p) = fixture();
        s.enemies[0].modes[3] = 1;
        s.enemies[0].frame = frame * 65536;
        tick_zombie_lunge(s, 0, p);
        ck(bool(p.flags & 4) == (frame >= 8 && frame < 13),
           "Capture frame window mismatch");
      }
      std::tie(s, p) = fixture();
      s.enemies[0].modes[3] = 1;
      s.enemies[0].frame = 12 * 65536;
      p.flags |= 4;
      p.model_work->joints[3].matrix[3] = NAN;
      tick_zombie_lunge(s, 0, p);
      ck(s.enemies[0].modes[3] == 2 && s.enemies[0].motion_number == 0,
         "Frame12 miss must enter recovery without capture cache read");
      s.enemies[0].motion_number = 122;
      s.enemies[0].frame = 22 * 65536;
      tick_zombie_lunge(s, 0, p);
      ck(s.enemies[0].modes[3] == 2, "Recovery must wait for final frame");
      s.enemies[0].frame = 23 * 65536;
      p.model_work.reset();
      tick_zombie_lunge(s, 0, p);
      ck(s.enemies[0].modes == std::array<uint8_t, 4>{1, 1, 1, 0} &&
             s.enemies[1].modes == s.enemies[0].modes,
         "Recovery pair mode copy failed");
      std::tie(s, p) = fixture();
      s.enemies[0].zombie_work->body.reset();
      s.enemies[0].flags |= 0x10000;
      tick_zombie_lunge(s, 0, p);
      ck(s.pending_enemy_sounds.empty(), "Suppressed voice emitted request");
      std::tie(s, p) = fixture();
      p.model_work->joints[0].matrix[3] = NAN;
      bad([&] { tick_zombie_lunge(s, 0, p); });
      ck(s.enemies[0].motion_number == 0 && s.enemies[1].motion_number == 0 &&
             s.enemies[0].modes[3] == 0 &&
             s.enemies[0].zombie_flags == 0x3000000 && p.flags == 1 &&
             s.pending_enemy_sounds.empty() && s.random_seed == 1,
         "Late steering failure must roll back paired state/voice");
      std::tie(s, p) = fixture();
      s.enemies[0].modes[3] = 1;
      s.enemies[0].frame = 8 * 65536;
      s.enemies[0].zombie_work->body = 127;
      bad([&] { tick_zombie_lunge(s, 0, p); });
      ck(p.flags == 1 && p.modes == std::array<uint8_t, 4>{} &&
             s.enemies[0].motion_number == 0 && s.enemies[0].modes[0] == 1,
         "Late body validation must roll back player grab state");
      std::tie(s, p) = fixture();
      s.enemies[0].flags |= 0x80;
      s.enemies[0].modes[3] = 1;
      s.enemies[0].frame = 7 * 65536;
      p.model_work->joints[0].matrix[3] = NAN;
      tick_zombie_lunge(s, 0, p);
      ck(s.enemies[0].angles[1] == 0 && s.enemies[0].modes[3] == 1,
         "Source skipped steering must not consume cached joint coordinates");
      std::tie(s, p) = fixture();
      s.enemies[0].modes[3] = 9;
      p.model_work.reset();
      tick_zombie_lunge(s, 0, p);
      ck(s.enemies[0].modes[3] == 9,
         "Unused submode consumed absent player cache");
    }
    std::cout << "Native zombie attack checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
