#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ashford;
namespace {
void ck(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
bool near(float a, float b) { return std::fabs(a - b) < .00002f; }
template <class F> void bad(F fn) {
  bool rejected = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Invalid failed-grab dependency accepted");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.collision.emplace();
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t n : {7u, 207u, 12u, 212u, 50u, 250u, 124u, 324u, 85u, 285u}) {
    Motion m;
    m.frames = 40;
    bank->at(n) = m;
  }
  auto &e = s.enemies[0];
  e.id = 1;
  e.flags = 0x21;
  e.modes = {2, 0, 1, 0};
  e.motions = bank;
  e.motion_number = 85;
  e.zombie_flags = 0x300010f;
  e.zombie_work.emplace();
  e.zombie_work->body = 1;
  e.health = 75;
  auto &b = s.enemies[1];
  b.id = 1;
  b.flags = 0x81;
  b.motions = bank;
  b.motion_number = 285;
  b.zombie_flags = 0x83000000;
  b.zombie_work.emplace();
  return s;
}
void wall(ScenarioState &s, bool cliff = false) {
  CollisionRecord w;
  w.flags = 1;
  w.position = {-10, 0, 2.5f};
  w.size = {20, 20, 1};
  s.collision->walls = {w};
  if (cliff) {
    CollisionRecord f;
    f.flags = 1;
    f.type = 2;
    f.position = {-20, 0, -20};
    f.size = {40, 0, 40};
    f.parameters = {1, 1, 2, 0};
    s.collision->floors = {f};
  }
}
void startup_and_recovery() {
  auto s = fixture();
  tick_zombie_failed_grab(s, 0);
  const auto &e = s.enemies[0];
  const auto &b = s.enemies[1];
  ck(e.motion_number == 7 && b.motion_number == 207 && e.frame == 16 * 65536 &&
         b.frame == 16 * 65536 && e.blend_count == 5 && b.blend_count == 5 &&
         e.motion_step == 65536 && b.motion_step == 65536 && e.modes[3] == 1 &&
         e.control0 == 0 && e.health == 75 && e.zombie_flags == 0x106 &&
         b.zombie_flags == 0x80000000 && e.flags == 0x40021,
     "NG01 frame16 paired startup/source flags wrong");
  s = fixture();
  auto &retained = s.enemies[0];
  auto &retained_body = s.enemies[1];
  retained.motion_number = 7;
  retained.frame = 16 * 65536;
  retained.blend_count = 9;
  retained.blend_rate = 123;
  retained.motion_mode = 0x108;
  retained.motion_step = 0;
  retained_body.motion_number = 207;
  retained_body.frame = 11 * 65536;
  retained_body.blend_count = 2;
  retained_body.blend_rate = 321;
  retained_body.motion_mode = 1;
  tick_zombie_failed_grab(s, 0);
  ck(retained.frame == 16 * 65536 && retained.blend_count == 9 &&
         retained.blend_rate == 123 && retained.motion_mode == 0x108 &&
         retained.motion_step == 65536 && retained_body.frame == 11 * 65536 &&
         retained_body.blend_count == 2 && retained_body.blend_rate == 321 &&
         retained_body.motion_mode == 1,
     "NG01 same-motion retention erased counters/mode");
  for (int frame : {17, 18, 30, 31}) {
    s = fixture();
    auto &root = s.enemies[0];
    root.modes[3] = 1;
    root.motion_number = 7;
    root.frame = frame * 65536;
    root.zombie_flags = 0;
    root.flags = 1;
    tick_zombie_failed_grab(s, 0);
    const bool recoil = frame == 18 || frame == 30;
    ck(near(root.position.z, recoil ? 1.5f : 0.f) &&
           root.control0 == (recoil ? 1 : 0) && !(root.flags & 0x40) &&
           !(root.zombie_flags & 0x100),
       "NG01 recoil window gained NG00 side effects");
  }
  s = fixture();
  auto &minimum = s.enemies[0];
  minimum.modes[3] = 1;
  minimum.motion_number = 7;
  minimum.frame = 20 * 65536;
  minimum.control0 = 11;
  tick_zombie_failed_grab(s, 0);
  ck(near(minimum.speed, .05f) && near(minimum.position.z, .05f) &&
         minimum.control0 == 12,
     "NG01 negative recoil speed did not clamp .05");
  s = fixture();
  auto &done = s.enemies[0];
  done.modes[3] = 1;
  done.motion_number = 7;
  done.frame = 39 * 65536;
  done.flags = 0x21;
  done.zombie_flags = 0x3000005;
  tick_zombie_failed_grab(s, 0);
  ck(done.motion_number == 12 && s.enemies[1].motion_number == 212 &&
         done.frame == 7 * 65536 && done.blend_count == 0 &&
         done.modes[3] == 2 && done.flags == 0x40021 && done.zombie_flags == 6,
     "NG01 terminal recovery gained flag40 or lost source frame7");
  done.frame = 19 * 65536;
  done.state_flags = 0x100000;
  tick_zombie_failed_grab(s, 0);
  ck(s.pending_enemy_sounds.empty(),
     "NG01 voice incorrectly used NG00 frame19");
  done.frame = 20 * 65536;
  tick_zombie_failed_grab(s, 0);
  ck(s.pending_enemy_sounds.size() == 1 &&
         s.pending_enemy_sounds[0].code == 8964 && s.random_seed == 1,
     "NG01 frame20 source sound/RNG wrong");
  done.frame = 39 * 65536;
  tick_zombie_failed_grab(s, 0);
  ck(done.modes == std::array<uint8_t, 4>{1, 0, 4, 0} &&
         s.enemies[1].modes == done.modes && done.motion_step == 0 &&
         s.enemies[1].motion_step == 0 && done.zombie_flags == 0x40006 &&
         done.zombie_secondary_flags == 0x40,
     "NG01 ordinary recovery did not stop paired walking works");
}
void wall_and_cliff() {
  for (bool falling : {false, true}) {
    auto s = fixture();
    wall(s, falling);
    auto &e = s.enemies[0];
    e.modes[3] = 1;
    e.motion_number = 7;
    e.frame = 18 * 65536;
    e.zombie_flags = 0;
    tick_zombie_failed_grab(s, 0);
    ck(e.motion_number == uint32_t(falling ? 124 : 50) &&
           s.enemies[1].motion_number == uint32_t(falling ? 324 : 250) &&
           e.modes[3] == (falling ? 4 : 3) && e.control0 == 5 &&
           near(e.zombie_work->action_anchor.x, 0) &&
           near(e.zombie_work->action_anchor.z, -.5f) && !(e.flags & 0x40) &&
           (e.flags & 0x20) == (falling ? 0u : 0x20u),
       "NG01 clipped recoil/wall/cliff pair wrong");
    e.zombie_work->action_anchor = {5, 0, -.5f};
    e.yaw_correction = 10;
    const float z = e.position.z;
    const int32_t yaw = e.angles[1];
    for (int step = 0; step < 5; ++step)
      tick_zombie_failed_grab(s, 0);
    ck(e.control0 == 0 && near(e.position.x, 5) &&
           near(e.position.z, z - .5f) && e.angles[1] == yaw + 40,
       "NG01 five-step correction/yaw first-frame gate wrong");
    e.frame = 5 * 65536;
    const auto before = e.position.z;
    tick_zombie_failed_grab(s, 0);
    ck(near(e.position.z,
            before + (falling ? std::cos(40 * 6.283185307179586 / 65536) * .5f
                              : 0.f)),
       "NG01 cliff fall displacement window wrong");
    e.frame = 39 * 65536;
    e.zombie_flags |= 0x200;
    tick_zombie_failed_grab(s, 0);
    ck(e.motion_step == 0 && s.enemies[1].motion_step == 0 &&
           e.modes == std::array<uint8_t, 4>{uint8_t(falling ? 4 : 1), 0,
                                             uint8_t(falling ? 3 : 4), 0} &&
           s.enemies[1].modes == e.modes &&
           (falling ? e.health == -1 : !(e.zombie_flags & 0x200)),
       "NG01 wall/cliff final paired modes/health wrong");
  }
}
void unused_and_rollback() {
  auto s = fixture();
  auto &e = s.enemies[0];
  e.modes[3] = 99;
  e.motions.reset();
  e.zombie_work->body = 255;
  s.collision.reset();
  tick_zombie_failed_grab(s, 0);
  ck(e.modes[3] == 99 && e.health == 75,
     "NG01 unknown state consumed missing context");
  s = fixture();
  s.enemies[0].zombie_work->body.reset();
  tick_zombie_failed_grab(s, 0);
  ck(s.enemies[0].motion_number == 7 && s.enemies[1].motion_number == 285,
     "NG01 null body not supported");
  s = fixture();
  wall(s, true);
  auto &late = s.enemies[0];
  late.modes[3] = 1;
  late.motion_number = 7;
  late.frame = 18 * 65536;
  s.collision->floors[0].parameters[2] = 255;
  bad([&] { tick_zombie_failed_grab(s, 0); });
  ck(late.position.z == 0 && late.speed == 0 && late.control0 == 0 &&
         late.motion_number == 7 && late.modes[3] == 1 &&
         s.enemies[1].motion_number == 285,
     "NG01 invalid cliff leaked recoil/pair mutation");
  s.collision->walls[0].attribute = 8;
  tick_zombie_failed_grab(s, 0);
  ck(late.modes[3] == 1 && near(late.position.z, 1.5f),
     "NG01 wall attr8 consumed unused cliff");
  s = fixture();
  auto bank = std::make_shared<MotionBank>(*s.enemies[0].motions);
  bank->at(12)->frames = 21;
  s.enemies[0].motions = bank;
  auto &sound = s.enemies[0];
  sound.modes[3] = 2;
  sound.motion_number = 12;
  sound.frame = 20 * 65536;
  sound.state_flags = 0x100000;
  sound.zombie_work->body = 255;
  bad([&] { tick_zombie_failed_grab(s, 0); });
  ck(s.pending_enemy_sounds.empty() && s.random_seed == 1 &&
         sound.motion_step == 65536 && sound.modes[3] == 2 &&
         sound.zombie_flags == 0x300010f && sound.zombie_secondary_flags == 0,
     "NG01 sound-before-late-body failure leaked queue/flags");
}
} // namespace
int main() {
  try {
    startup_and_recovery();
    wall_and_cliff();
    unused_and_rollback();
    std::cout << "Native zombie failed-grab recovery checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
