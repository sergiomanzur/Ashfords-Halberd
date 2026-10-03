#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace ashford;
namespace {
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
template <class F> void bad(F fn) {
  bool failed = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Invalid consumed enemy grab dependency accepted");
}
bool near(float a, float b) { return std::fabs(a - b) < .00002f; }
std::pair<ScenarioState, ScenarioEntity> fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.collision.emplace();
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t n : {8u, 208u, 120u, 320u, 7u, 207u, 12u, 212u, 34u, 234u, 111u,
                     311u, 50u, 250u, 124u, 324u, 85u, 285u}) {
    Motion m;
    m.frames = 80;
    bank->at(n) = m;
  }
  auto &e = s.enemies[0];
  e.id = 1;
  e.flags = 0x41;
  e.modes = {2, 0, 0, 0};
  e.zombie_work.emplace();
  e.zombie_work->body = 1;
  e.zombie_work->personality = 0;
  e.motion_number = 85;
  e.motions = bank;
  e.health = -1;
  e.zombie_flags = 0x7000000;
  e.secondary_flags = 4;
  e.model_work.emplace();
  e.model_work->joints.resize(18);
  e.joint_pose.resize(18);
  e.joint_pose[10].angles = std::array<int32_t, 3>{0, 1024, 0};
  auto &b = s.enemies[1];
  b.id = 1;
  b.flags = 0x81;
  b.motions = bank;
  b.zombie_work.emplace();
  b.zombie_flags = 0x83000000;
  ScenarioEntity p;
  p.flags = 0x10005;
  p.health = 100;
  p.position = {0, 0, -4};
  p.modes = {4, 0, 0, 4};
  p.control1 = 11;
  return {s, p};
}
void wall(ScenarioState &s, bool cliff = false) {
  CollisionRecord w;
  w.flags = 1;
  w.position = {-10, 0, 2.5f};
  w.size = {20, 20, 1};
  s.collision->walls = {w};
  if (cliff) {
    CollisionRecord floor;
    floor.flags = 1;
    floor.type = 2;
    floor.position = {-20, 0, -20};
    floor.size = {40, 0, 40};
    floor.parameters = {1, 1, 2, 0};
    s.collision->floors = {floor};
  }
}
} // namespace
int main() {
  try {
    auto [s, p] = fixture();
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].motion_number == 8 && s.enemies[1].motion_number == 208 &&
           s.enemies[0].modes[3] == 1 && s.enemies[0].health == 0 &&
           s.enemies[0].secondary_flags == 5 &&
           p.modes == std::array<uint8_t, 4>{4, 0, 0, 0} &&
           near(s.enemies[0].position.z, 2.290813f) &&
           s.enemies[0].zombie_flags == 0x20005,
       "Grab startup pair/player positioning/flags wrong");
    for (uint8_t chr : {1, 3}) {
      std::tie(s, p) = fixture();
      s.player_id = chr;
      s.enemies[0].zombie_flags |= 0x4000;
      tick_zombie_grab(s, 0, p);
      ck(s.enemies[0].motion_number == 120 &&
             s.enemies[1].motion_number == 320 &&
             near(s.enemies[0].position.z, 2.442887f),
         "Chris grab pair/back offset wrong");
    }
    std::tie(s, p) = fixture();
    auto &e = s.enemies[0];
    e.modes[3] = 1;
    e.frame = 20 * 65536;
    tick_zombie_grab(s, 0, p);
    ck(e.modes[3] == 2 && e.control0 == 60,
       "Grab frame20 timer transition wrong");
    e.frame = 25 * 65536;
    e.control0 = 60;
    tick_zombie_grab(s, 0, p);
    ck(p.health == 90 && e.control0 == 59 && e.modes[3] == 2,
       "Bite damage-before-timer wrong");
    e.frame = 60 * 65536;
    tick_zombie_grab(s, 0, p);
    ck(p.health == 80, "Second authored bite missing");
    s.input_held = 0x2d0f;
    s.input_pressed = 0;
    e.frame = 24 * 65536;
    e.control0 = 10;
    tick_zombie_grab(s, 0, p);
    ck(e.control0 == 9, "Held input must not count as escape presses");
    s.input_pressed = 0x2d0f;
    e.control0 = 9;
    tick_zombie_grab(s, 0, p);
    ck(e.control0 == 1,
       "Escape directional/action presses must total8 with natural tick");
    tick_zombie_grab(s, 0, p);
    ck(e.modes[3] == 3 && e.motion_number == 7 &&
           s.enemies[1].motion_number == 207 && p.modes[3] == 2 &&
           p.control1 == 0 && e.control0 == 0 && (e.flags & 0x40020) &&
           !(e.secondary_flags & 1),
       "Input escape state/pair/flags wrong");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 2;
    s.enemies[0].frame = 25 * 65536;
    s.enemies[0].control0 = 1;
    p.health = 10;
    tick_zombie_grab(s, 0, p);
    ck(p.health == 0 && p.modes[3] == 2 && !(p.state_flags & 0x40000),
       "Health0 must survive authored bite");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 2;
    s.enemies[0].frame = 25 * 65536;
    s.enemies[0].control0 = 1;
    p.health = 9;
    tick_zombie_grab(s, 0, p);
    ck(p.health == -1 && p.modes[0] == 6 && p.modes[3] == 6 &&
           (p.state_flags & 0x40000) && s.enemies[0].motion_number == 34 &&
           s.enemies[1].motion_number == 234 && s.enemies[0].modes[3] == 6,
       "Fatal bite and synchronized enemy/player states wrong");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 3;
    s.enemies[0].frame = 18 * 65536;
    tick_zombie_grab(s, 0, p);
    ck(near(s.enemies[0].position.z, 1.5f) && s.enemies[0].control0 == 1 &&
           s.enemies[0].speed == 1.5f,
       "Escape recoil movement wrong");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 3;
    s.enemies[0].frame = 79 * 65536;
    s.enemies[0].motion_number = 7;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].motion_number == 12 && s.enemies[1].motion_number == 212 &&
           s.enemies[0].frame == 7 * 65536 && s.enemies[0].modes[3] == 4,
       "Normal recoil recovery pair/frame wrong");
    s.enemies[0].frame = 79 * 65536;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].modes == std::array<uint8_t, 4>{1, 0, 4, 0} &&
           s.enemies[1].modes == s.enemies[0].modes &&
           s.enemies[0].motion_step == 0 && s.enemies[1].motion_step == 0 &&
           (s.enemies[0].zombie_secondary_flags & 0x40),
       "Recovery must stop both motion works and restore damage-response mode");
    std::tie(s, p) = fixture();
    wall(s);
    s.enemies[0].modes[3] = 3;
    s.enemies[0].frame = 18 * 65536;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].modes[3] == 5 && s.enemies[0].motion_number == 50 &&
           s.enemies[1].motion_number == 250 && s.enemies[0].control0 == 5 &&
           s.enemies[0].yaw_correction == 0,
       "Wall recoil must choose source wall recovery");
    const float before = s.enemies[0].position.z;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].control0 == 4 &&
           near(s.enemies[0].position.z, before - .1f),
       "First wall-correction tick must translate anchor/5 without turning");
    std::tie(s, p) = fixture();
    wall(s, true);
    s.enemies[0].modes[3] = 3;
    s.enemies[0].frame = 18 * 65536;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].modes[3] == 8 && s.enemies[0].motion_number == 124 &&
           s.enemies[1].motion_number == 324 && !(s.enemies[0].flags & 0x20),
       "Cliff-facing recoil must choose fall branch");
    s.enemies[0].frame = 79 * 65536;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].health == -1 &&
           s.enemies[0].modes == std::array<uint8_t, 4>{4, 0, 3, 0} &&
           s.enemies[1].modes == s.enemies[0].modes,
       "Cliff ending must enter original death state");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 3;
    s.enemies[0].frame = 20 * 65536;
    s.enemies[0].zombie_work->children[5] = 2;
    auto &cap = s.enemies[2];
    cap.id = 34;
    cap.flags = 0x81;
    cap.model_work.emplace();
    cap.model_work->joints.resize(1);
    s.enemies[0].floor = 2;
    tick_zombie_grab(s, 0, p);
    ck(cap.flags == 1 && (cap.model_work->joints[0].flags & 3) == 3 &&
           cap.turn_speed == -31744 && cap.torso_yaw == 0x800 &&
           cap.roll_speed == 0x1800 && cap.grab_yaw_offset == 0 &&
           cap.damage_position.x == .6f && cap.floor == 2 && cap.modes[0] == 3,
       "Cap flight source metadata/widths wrong");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 6;
    s.enemies[0].flags |= 0x2000000;
    s.enemies[0].model_work->joints[0].matrix[3] = 7;
    s.enemies[0].model_work->joints[0].matrix[11] = 9;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].position.x == 7 && s.enemies[0].position.z == 9 &&
           s.enemies[0].motion_number == 111 &&
           s.enemies[1].motion_number == 311 && s.enemies[0].modes[3] == 7,
       "Fatal enemy completion must relocate from source cached root");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 2;
    s.enemies[0].frame = 25 * 65536;
    s.enemies[0].control0 = 1;
    s.enemies[0].zombie_work->body = 999;
    bad([&] { tick_zombie_grab(s, 0, p); });
    ck(p.health == 100 && s.enemies[0].control0 == 1 && p.modes[3] == 4,
       "Late invalid paired body leaked bite/timer/player modes");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 3;
    s.enemies[0].frame = 20 * 65536;
    s.enemies[0].zombie_work->children[5] = 999;
    bad([&] { tick_zombie_grab(s, 0, p); });
    ck(s.enemies[0].position.z == 0 && s.enemies[0].control0 == 0 &&
           !(s.enemies[0].zombie_flags & 0x100),
       "Invalid cap leaked recoil or event flags");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 7;
    s.collision.reset();
    p.position.x = NAN;
    s.enemies[0].zombie_work->body = 999;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].modes[3] == 7,
       "Unused grab case consumed unrelated context");
    std::tie(s, p) = fixture();
    s.enemies[0].motion_number = 8;
    s.enemies[0].frame = 20 * 65536;
    s.enemies[0].blend_count = 6;
    s.enemies[0].blend_rate = 300;
    s.enemies[0].motion_mode = 0x55;
    s.enemies[0].motion_step = 0;
    s.enemies[1].motion_number = 208;
    s.enemies[1].frame = 5 * 65536;
    s.enemies[1].blend_count = 8;
    s.enemies[1].motion_mode = 0x66;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].frame == 20 * 65536 && s.enemies[0].blend_count == 6 &&
           s.enemies[0].blend_rate == 300 && s.enemies[0].motion_mode == 0x55 &&
           s.enemies[0].motion_step == 65536 && s.enemies[0].modes[3] == 2 &&
           s.enemies[0].control0 == 60 && s.enemies[1].frame == 5 * 65536 &&
           s.enemies[1].blend_count == 8 && s.enemies[1].motion_mode == 0x66,
       "Same-motion grab startup must retain counters and source fallthrough");
    std::tie(s, p) = fixture();
    s.enemies[0].zombie_work->body.reset();
    s.enemies[1].flags = 0;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].motion_number == 8 && s.enemies[1].motion_number == 0,
       "Source null-body grab must skip paired context");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 2;
    s.enemies[0].frame = 25 * 65536;
    s.enemies[0].control0 = 1;
    s.enemies[0].zombie_work->personality = 1;
    p.health = 16;
    tick_zombie_grab(s, 0, p);
    ck(p.health == 1 && p.modes[3] == 2,
       "Grab bite must include source personality attack bonus");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 9;
    s.enemies[0].frame = 79 * 65536;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].modes == std::array<uint8_t, 4>{1, 0, 4, 0} &&
           s.enemies[1].modes == s.enemies[0].modes &&
           (s.enemies[0].zombie_flags & 0x40000) &&
           (s.enemies[0].zombie_secondary_flags & 0x40),
       "Alternate grab recovery must restore paired source modes");
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 4;
    s.enemies[0].frame = 19 * 65536;
    s.enemies[0].state_flags = 0x100000;
    tick_zombie_grab(s, 0, p);
    ck(s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].code == 8964 && s.random_seed == 1,
       "Source recovery vocalization must retain sound code/RNG");
    std::tie(s, p) = fixture();
    wall(s, true);
    s.collision->floors[0].parameters[2] = 255;
    s.enemies[0].modes[3] = 3;
    s.enemies[0].frame = 20 * 65536;
    s.enemies[0].zombie_work->children[5] = 2;
    s.enemies[2].id = 34;
    s.enemies[2].flags = 0x81;
    s.enemies[2].model_work.emplace();
    s.enemies[2].model_work->joints.resize(1);
    bad([&] { tick_zombie_grab(s, 0, p); });
    ck(s.enemies[0].position.z == 0 && s.enemies[0].control0 == 0 &&
           s.enemies[2].flags == 0x81 && s.enemies[2].modes[0] == 0,
       "Late cliff-query failure must roll back staged recoil and cap flight");
    s.collision->walls[0].attribute = 8;
    tick_zombie_grab(s, 0, p);
    ck(s.enemies[0].modes[3] == 3 && s.enemies[2].modes[0] == 3 &&
           (s.enemies[0].zombie_flags & 0x100),
       "Wall attribute8 must skip unused cliff context while preserving cap "
       "event");
    std::cout << "Native zombie grab checks passed\n";
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
