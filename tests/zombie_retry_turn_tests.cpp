#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
using namespace ashford;
namespace {
void ck(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Malformed consumed retry/turn context did not reject");
}
std::pair<ScenarioState, ScenarioEntity> fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  auto &e = s.enemies[0];
  e.flags = 0x41;
  e.id = 1;
  e.modes = {1, 0, 6, 0};
  e.zombie_work.emplace();
  e.zombie_work->body = 1;
  e.zombie_flags = 0x3000010;
  e.model_work.emplace();
  e.model_work->joints.resize(11);
  for (auto &j : e.model_work->joints)
    j.matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  auto &b = s.enemies[1];
  b.flags = 0x81;
  b.id = 1;
  b.zombie_work.emplace();
  b.zombie_flags = 0x83000010;
  b.modes = {1, 7, 9, 3};
  ScenarioEntity p;
  p.flags = 1;
  p.health = 100;
  p.position = {0, 0, -4};
  p.modes = {7, 6, 5, 4};
  p.model_work = e.model_work;
  for (auto &j : p.model_work->joints)
    j.matrix[11] = -4;
  return {s, p};
}
void unchanged(const ScenarioState &s, const ScenarioEntity &p) {
  ck(s.enemies[0].motion_number == 0 && s.enemies[1].motion_number == 0 &&
         s.enemies[0].zombie_flags == 0x3000010 &&
         s.enemies[1].zombie_flags == 0x83000010 &&
         s.enemies[0].flags == 0x41 && s.enemies[1].flags == 0x81 &&
         s.enemies[0].modes[3] == 0 && s.random_seed == 1 && p.flags == 1,
     "Late failure partially committed retry/turn state");
}
void retry() {
  auto [s, p] = fixture();
  p.flags = 5;
  tick_zombie_capture_retry(s, 0, p);
  const auto &e = s.enemies[0];
  const auto &b = s.enemies[1];
  ck(e.motion_number == 49 && b.motion_number == 249 && e.frame == 2949120 &&
         b.frame == 2949120 && e.blend_count == 15 && b.blend_count == 15 &&
         e.zombie_flags == 0x10 && b.zombie_flags == 0x80000010 &&
         e.flags == 0x40041 && b.flags == 0x81 && e.control0 == 15 &&
         s.random_seed == 1103527590u &&
         e.modes == std::array<uint8_t, 4>{1, 1, 6, 1} &&
         b.modes == std::array<uint8_t, 4>{1, 7, 9, 3} && p.flags == 5,
     "Source retry startup/fallthrough/pair/RNG mismatch");
  tick_zombie_capture_retry(s, 0, p);
  ck(s.random_seed == 1103527590u && e.control0 == 15 && e.modes[3] == 1,
     "Retry waiting unexpectedly decremented timer or rolled RNG");
  std::tie(s, p) = fixture();
  s.enemies[0].angles[1] = 5000;
  tick_zombie_capture_retry(s, 0, p);
  ck(s.enemies[0].angles[1] == 4090 &&
         s.enemies[0].modes == std::array<uint8_t, 4>{2, 0, 0, 0} &&
         s.enemies[1].modes == s.enemies[0].modes && p.flags == 0x10005 &&
         p.modes == std::array<uint8_t, 4>{7, 6, 5, 4} &&
         s.enemies[0].zombie_flags == 0x90,
     "Retry capture must consume staged yaw and preserve player modes");
  for (int gate = 0; gate < 5; ++gate) {
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 1;
    if (gate == 0)
      p.model_work->joints[3].matrix[11] = -5.5;
    if (gate == 1)
      p.position.y = .01f;
    if (gate == 2)
      p.health = -1;
    if (gate == 3)
      p.flags = 2;
    if (gate == 4)
      s.enemies[0].zombie_flags |= 0x20000000;
    tick_zombie_capture_retry(s, 0, p);
    ck(s.enemies[0].modes == std::array<uint8_t, 4>{1, 0, 5, 0} &&
           s.enemies[1].modes == std::array<uint8_t, 4>{1, 7, 9, 3} &&
           !(p.flags & 4) && s.random_seed == 1,
       "Retry capture rejection must enter lunge without body mode copy");
  }
  std::tie(s, p) = fixture();
  p.health = 0;
  s.enemies[0].zombie_work->body.reset();
  tick_zombie_capture_retry(s, 0, p);
  ck(s.enemies[0].modes[0] == 2 && p.flags == 0x10005,
     "Unlinked/zero health capture failed");
  std::tie(s, p) = fixture();
  s.enemies[0].motion_number = 49;
  s.enemies[1].motion_number = 249;
  for (auto i : {0, 1}) {
    auto &w = s.enemies[i];
    w.frame = 123;
    w.blend_count = 99;
    w.motion_mode = 0x44;
    w.flags |= 0x2000000;
    w.motion_step = 0;
  }
  p.flags = 5;
  tick_zombie_capture_retry(s, 0, p);
  for (auto i : {0, 1})
    ck(s.enemies[i].frame == 123 && s.enemies[i].blend_count == 99 &&
           s.enemies[i].motion_mode == 0x44 &&
           s.enemies[i].motion_step == 65536 &&
           (s.enemies[i].flags & 0x2000000),
       "Same-motion source shortcut was lost");
  std::tie(s, p) = fixture();
  p.position.x = NAN;
  bad([&] { tick_zombie_capture_retry(s, 0, p); });
  unchanged(s, p);
  std::tie(s, p) = fixture();
  p.model_work.reset();
  bad([&] { tick_zombie_capture_retry(s, 0, p); });
  unchanged(s, p);
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 1;
  s.enemies[0].zombie_work->body = 127;
  bad([&] { tick_zombie_capture_retry(s, 0, p); });
  ck(p.flags == 1 && s.enemies[0].modes[0] == 1 && s.random_seed == 1,
     "Late capture body failure committed player/modes");
  for (size_t index : {size_t(0), size_t(128)}) {
    std::tie(s, p) = fixture();
    s.enemies[0].zombie_work->body = index;
    bad([&] { tick_zombie_capture_retry(s, 0, p); });
    unchanged(s, p);
  }
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 1;
  s.enemies[0].flags |= 0x80;
  s.enemies[0].zombie_work->body = 127;
  p.flags = 5;
  p.position = {NAN, NAN, NAN};
  p.model_work.reset();
  tick_zombie_capture_retry(s, 0, p);
  ck(s.enemies[0].modes[3] == 1 && s.random_seed == 1,
     "Suppressed retry consumed unused data");
  std::tie(s, p) = fixture();
  p.flags = 5;
  p.position.y = NAN;
  p.model_work.reset();
  tick_zombie_capture_retry(s, 0, p);
  ck(s.enemies[0].modes[3] == 1, "Blocked retry turn must ignore Y/cache");
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 9;
  p.position = {NAN, NAN, NAN};
  p.model_work.reset();
  tick_zombie_capture_retry(s, 0, p);
  ck(s.random_seed == 1 && s.enemies[0].motion_number == 0,
     "Unused retry submode consumed dependencies");
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 1;
  s.enemies[0].angles[1] = std::numeric_limits<int32_t>::max();
  p.flags = 5;
  p.position = {-4, 0, 0};
  tick_zombie_capture_retry(s, 0, p);
  ck(s.enemies[0].angles[1] == std::numeric_limits<int32_t>::min() + 909,
     "Retry yaw addition must wrap the source32bit word");
  std::tie(s, p) = fixture();
  bad([&] { tick_zombie_capture_retry(s, 0, s.enemies[2]); });
  unchanged(s, p);
}
void turn() {
  auto [s, p] = fixture();
  p.position = {-4, NAN, 0};
  tick_zombie_turn(s, 0, p);
  ck(s.enemies[0].motion_number == 127 && s.enemies[1].motion_number == 327 &&
         s.enemies[0].yaw_correction == 744 &&
         s.enemies[0].motion_mode == 0x20 && s.enemies[1].motion_mode == 0x20 &&
         s.enemies[0].blend_count == 8 && s.enemies[0].flags == 0x40041 &&
         s.enemies[1].flags == 0x81 && s.enemies[0].zombie_flags == 0x10 &&
         s.enemies[1].zombie_flags == 0x80000010 &&
         s.enemies[0].modes[3] == 1 && s.random_seed == 1,
     "Positive authored turn startup mismatch");
  std::tie(s, p) = fixture();
  p.position = {4, 0, 0};
  tick_zombie_turn(s, 0, p);
  ck(s.enemies[0].yaw_correction == -744 && s.enemies[0].motion_mode == 0x22 &&
         s.enemies[1].motion_mode == 0x22,
     "Negative authored turn reverse/truncation mismatch");
  for (int delta : {32768, 32769}) {
    std::tie(s, p) = fixture();
    s.enemies[0].angles[1] = -delta;
    tick_zombie_turn(s, 0, p);
    ck(s.enemies[0].yaw_correction == (delta == 32768 ? 1489 : -1489) &&
           bool(s.enemies[0].motion_mode & 2) == (delta == 32769),
       "Half-turn source threshold mismatch");
  }
  for (int frame : {16, 17, 39, 40, 41, -1}) {
    std::tie(s, p) = fixture();
    auto &e = s.enemies[0];
    e.modes = {1, 0, 7, 1};
    e.frame = frame * 65536 + 123;
    e.yaw_correction = -744;
    e.angles = {4, 10000, 6};
    p.position = {NAN, NAN, NAN};
    s.enemies[0].zombie_work->body = 127;
    tick_zombie_turn(s, 0, p);
    ck(e.angles[1] == (frame >= 17 && frame < 40 ? 9256 : 10000) &&
           e.angles[0] == 4 && e.angles[2] == 6 &&
           e.modes == (frame == 40 ? std::array<uint8_t, 4>{1, 1, 1, 0}
                                   : std::array<uint8_t, 4>{1, 0, 7, 1}) &&
           s.enemies[1].modes == std::array<uint8_t, 4>{1, 7, 9, 3},
       "Authored turn frame window/exact return/body mode preservation "
       "mismatch");
  }
  std::tie(s, p) = fixture();
  auto &e = s.enemies[0];
  e.modes[3] = 1;
  e.frame = 17 * 65536;
  e.angles[1] = std::numeric_limits<int32_t>::max();
  e.yaw_correction = 1;
  tick_zombie_turn(s, 0, p);
  ck(e.angles[1] == std::numeric_limits<int32_t>::min(),
     "Source yaw word must wrap32bits");
  std::tie(s, p) = fixture();
  s.enemies[0].zombie_work->body.reset();
  p.position = {4, 0, 0};
  tick_zombie_turn(s, 0, p);
  ck(s.enemies[0].yaw_correction == -744, "Unlinked turn failed");
  std::tie(s, p) = fixture();
  p.position = {-4, 0, 0};
  s.enemies[0].motion_number = 127;
  s.enemies[1].motion_number = 327;
  for (auto i : {0, 1}) {
    auto &w = s.enemies[i];
    w.frame = 17 * 65536;
    w.blend_count = 99;
    w.motion_mode = 0x42;
    w.flags |= 0x2000000;
    w.motion_step = 0;
  }
  tick_zombie_turn(s, 0, p);
  ck(s.enemies[0].angles[1] == 744,
     "Same turn motion must fall through at the retained incoming frame");
  for (auto i : {0, 1})
    ck(s.enemies[i].frame == 17 * 65536 && s.enemies[i].blend_count == 99 &&
           s.enemies[i].motion_mode == 0x42 &&
           s.enemies[i].motion_step == 65536 &&
           (s.enemies[i].flags & 0x2000000),
       "Same turn motion must retain prior direction/blend/flag state");
  std::tie(s, p) = fixture();
  p.position.x = NAN;
  bad([&] { tick_zombie_turn(s, 0, p); });
  unchanged(s, p);
  std::tie(s, p) = fixture();
  s.enemies[0].zombie_work->body = 0;
  bad([&] { tick_zombie_turn(s, 0, p); });
  unchanged(s, p);
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 9;
  p.position = {NAN, NAN, NAN};
  tick_zombie_turn(s, 0, p);
  ck(s.enemies[0].motion_number == 0 && s.random_seed == 1,
     "Unused turn submode consumed target");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--turn")
      turn();
    else
      retry();
    std::cout << "Native zombie retry/turn checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
