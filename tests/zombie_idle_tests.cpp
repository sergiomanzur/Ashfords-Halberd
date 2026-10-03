#include "zombie.hpp"
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
  bool caught = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  ck(caught, "Invalid idle context did not reject");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  auto &e = s.enemies[0];
  e.id = 1;
  e.flags = 0x2040001;
  e.modes = {1, 0, 0, 0};
  e.zombie_work.emplace();
  e.zombie_work->body = 1;
  e.zombie_work->personality = 1;
  e.zombie_flags = 0x300040f;
  e.control1 = 222;
  e.control2 = 333;
  e.motion_number = 9;
  e.frame = 17;
  e.blend_count = 17;
  e.blend_rate = 123;
  e.motion_step = 5;
  e.motion_mode = 0x108;
  auto bank = std::make_shared<MotionBank>(203);
  for (auto no : {2, 9, 202}) {
    Motion m;
    m.frames = no == 9 ? 13 : 30;
    (*bank)[no] = m;
  }
  e.motions = bank;
  auto &b = s.enemies[1];
  b.id = 1;
  b.flags = 0x2040081;
  b.zombie_work.emplace();
  b.zombie_flags = 0x83000000;
  b.motion_number = 201;
  b.motions = bank;
  return s;
}
int main() {
  try {
    auto s = fixture();
    tick_zombie_idle(s, 0);
    auto &e = s.enemies[0];
    auto &b = s.enemies[1];
    ck(e.motion_number == 2 && b.motion_number == 202 && e.frame == 9 * 65536 &&
           b.frame == e.frame && e.control0 == 114 && e.control3 == 63 &&
           e.control1 == 222 && e.control2 == 333 &&
           e.modes == std::array<uint8_t, 4>{1, 1, 0, 1} &&
           s.random_seed == 662824084 && e.motion_step == 65536 &&
           e.blend_count == 15 && e.blend_rate == 0 && e.motion_mode == 0x20 &&
           e.flags == 0x40001 && e.zombie_flags == 1 &&
           b.zombie_flags == 0x80000000 && s.pending_enemy_sounds.empty(),
       "Original idle startup frame/RNG/fallthrough/body/flags failed");
    e.control0 = 1;
    tick_zombie_idle(s, 0);
    ck(e.control0 == 0 && e.modes == std::array<uint8_t, 4>{1, 1, 1, 0} &&
           e.control3 == 62 && s.random_seed == 662824084,
       "Idle countdown transition changed");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control0 = 100;
    s.enemies[0].control3 = 1;
    s.enemies[0].zombie_flags = 0x400;
    tick_zombie_idle(s, 0);
    ck(s.enemies[0].modes[2] == 1 && s.enemies[0].modes[3] == 0 &&
           s.random_seed == 1,
       "Notice should select walking without consuming RNG");
    s = fixture();
    s.enemies[0].type = 8;
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control0 = 0;
    s.enemies[0].control3 = 1;
    tick_zombie_idle(s, 0);
    ck(s.enemies[0].modes[2] == 0 && s.enemies[0].modes[3] == 1 &&
           s.enemies[0].control0 == -1,
       "Type8 must stay idle after timer/notice");
    s = fixture();
    s.enemies[0].modes[3] = 2;
    s.enemies[0].control3 = 2;
    s.enemies[1].zombie_work->neck_yaw = 9000;
    tick_zombie_idle(s, 0);
    ck(s.enemies[1].zombie_work->neck_yaw == 9102 &&
           s.enemies[0].modes[3] == 3 && s.enemies[0].control3 == 1,
       "Original positive neck oscillation clamp failed");
    s.enemies[1].zombie_work->neck_yaw = 56500;
    tick_zombie_idle(s, 0);
    ck(s.enemies[1].zombie_work->neck_yaw == 56434 &&
           s.enemies[0].modes[3] == 2 && s.enemies[0].control3 == 0,
       "Original negative neck oscillation clamp failed");
    s = fixture();
    s.enemies[0].modes[3] = 99;
    s.enemies[0].control3 = 0;
    s.enemies[0].position = {1, 2, 3};
    tick_zombie_idle(s, 0);
    ck(s.enemies[0].modes[3] == 99 && s.enemies[0].control3 == 210 &&
           s.random_seed == 1103527590 && s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].work == 0 &&
           s.pending_enemy_sounds[0].code == 16847671 &&
           s.pending_enemy_sounds[0].position.x == 1,
       "Idle sound cadence/personality offset/native request changed");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control0 = std::numeric_limits<int32_t>::min();
    s.enemies[0].control3 = std::numeric_limits<int32_t>::min();
    s.enemies[0].zombie_flags = 0;
    tick_zombie_idle(s, 0);
    ck(s.enemies[0].control0 == std::numeric_limits<int32_t>::max() &&
           s.enemies[0].control3 == std::numeric_limits<int32_t>::max() &&
           s.random_seed == 1,
       "Source signed counter word wrap changed");
    s = fixture();
    s.enemies[0].zombie_work->body.reset();
    tick_zombie_idle(s, 0);
    ck(s.enemies[0].motion_number == 2,
       "Unlinked root without body must still idle");
    for (int invalid = 0; invalid < 5; ++invalid) {
      s = fixture();
      if (invalid == 0)
        s.enemies[0].zombie_work->body = 128;
      if (invalid == 1)
        s.enemies[1].zombie_work.reset();
      if (invalid == 2)
        s.enemies[0].motions.reset();
      if (invalid == 3) {
        auto bank = std::make_shared<MotionBank>(*s.enemies[0].motions);
        (*bank)[9]->frames = 0;
        s.enemies[0].motions = bank;
      }
      if (invalid == 4) {
        s.enemies[0].modes[3] = 99;
        s.enemies[0].control3 = 0;
        s.enemies[0].zombie_work->personality = 255;
      }
      bad([&] { tick_zombie_idle(s, 0); });
      ck(s.random_seed == 1 && s.enemies[0].frame == 17 &&
             s.enemies[0].motion_number == 9 &&
             s.enemies[1].motion_number == 201 &&
             s.pending_enemy_sounds.empty(),
         "Invalid idle context partially committed scalars/RNG");
    }
    ScenarioEntity motion;
    motion.motion_number = 2;
    motion.frame = 123;
    motion.blend_count = 7;
    motion.blend_rate = 99;
    motion.motion_step = 1;
    motion.motion_mode = 0x108;
    motion.flags = 0x2040123;
    ck(change_enemy_motion(motion, 2, 999, 99) == -1 &&
           motion.motion_step == 65536 && motion.frame == 123 &&
           motion.blend_count == 7 && motion.blend_rate == 99 &&
           motion.motion_mode == 0x108 && motion.flags == 0x2040123,
       "Unchanged motion must preserve all fields except step");
    ck(change_enemy_motion(motion, 202, -65536, 15) == 0 &&
           motion.motion_number == 202 && motion.frame == -65536 &&
           motion.blend_count == 15 && motion.blend_rate == 0 &&
           motion.motion_mode == 0x20 && motion.flags == 0x123,
       "Changed motion reset/flag mask failed");
    auto z = fixture().enemies[0];
    uint32_t seed = 1;
    ck(choose_zombie_sound(z, seed, 16847639, false) == 16847671 && seed == 1,
       "Original voice personality offset changed");
    z.flags |= 0x10000;
    z.zombie_work->personality = 255;
    ck(!choose_zombie_sound(z, seed, 20, false) && seed == 1,
       "Silent flag must bypass unused personality/RNG");
    z.flags &= ~0x10000;
    z.zombie_work->personality = 1;
    ck(!choose_zombie_sound(z, seed, 20, true) && seed == 1,
       "Already playing voice must short-circuit RNG");
    ck(!choose_zombie_sound(z, seed, 20, false) && seed == 1103527590,
       "Even random roll must suppress voice");
    ck(choose_zombie_sound(z, seed, 21, false) == 53 && seed == 2524885223,
       "Odd voice roll/offset failed");
    z.state_flags = 0x100000;
    const auto voice_seed = seed;
    ck(!choose_zombie_sound(z, seed, 21, std::vector<int32_t>{53}) &&
           seed == voice_seed,
       "Resolved active voice code must bypass RNG");
    ck(!choose_zombie_sound(z, seed, 21, std::vector<int32_t>{8963}) &&
           seed == voice_seed,
       "Any active sound in the same slot must suppress a positive voice query "
       "without RNG");
    ck(choose_zombie_sound(z, seed, 70414, false) == 70416 &&
           choose_zombie_sound(z, seed, 70415, false) == 70417 &&
           !choose_zombie_sound(z, seed, 8963, false),
       "Source special sound status changed");
    z.zombie_work->personality = 255;
    auto prior = seed;
    bad([&] { choose_zombie_sound(z, seed, 20, false); });
    ck(seed == prior, "Invalid voice context consumed RNG");
    std::cout << "Native zombie idle controller tests passed\n";
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
