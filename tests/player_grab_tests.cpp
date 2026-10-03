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
  ck(failed, "Invalid consumed grab dependency accepted");
}
std::pair<ScenarioState, ScenarioEntity> fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t number : {406u, 407u, 408u, 409u, 410u, 411u, 412u, 413u,
                          431u, 432u, 433u, 434u, 435u, 436u, 437u, 438u,
                          456u, 457u, 458u, 459u, 460u, 461u, 462u, 463u}) {
    Motion motion;
    motion.frames = 40;
    bank->at(number) = motion;
  }
  auto &e = s.enemies[0];
  e.flags = 0x41;
  e.id = 1;
  e.zombie_work.emplace();
  e.zombie_work->body = 1;
  e.zombie_flags = 0x20000;
  e.motions = bank;
  e.zombie_work->personality = 1;
  s.enemies[1].id = 1;
  s.enemies[1].flags = 0x81;
  ScenarioEntity p;
  p.flags = 0x50014;
  p.state_flags = 0x10000;
  p.attack_flags = 0xdead;
  p.modes = {4, 0, 0, 0};
  p.player_work.emplace();
  p.player_work->status_flags = 0x100004;
  p.motions = std::make_shared<MotionBank>(512);
  p.base_motions = p.motions;
  p.angles[1] = 65530;
  s.input_held = 0xabcd;
  return {s, p};
}
void push_tests() {
  auto [s, p] = fixture();
  auto &e = s.enemies[2];
  e.id = 1;
  e.flags = 1;
  e.type = 0;
  e.modes = {1, 1, 1, 5};
  e.position = {0, 0, -9};
  e.zombie_work.emplace();
  e.angles[1] = 0;
  p.angles[1] = 0;
  push_zombie_grab_neighbors(s, 0, p, 0);
  ck(e.modes == std::array<uint8_t, 4>{3, 0, 2, 0} && e.zombie_flags == 0x200 &&
         s.enemies[1].modes[0] == 3,
     "Front grab neighbor push/body side effect wrong");
  ck(s.random_seed == 1, "Unconditional push consumed RNG");
  std::tie(s, p) = fixture();
  auto add = [&](size_t slot, Vec3 pos, uint16_t id = 1) {
    auto &other = s.enemies[slot];
    other.id = id;
    other.flags = 1;
    other.zombie_work.emplace();
    other.modes = {1, 9, 9, 9};
    other.position = pos;
  };
  add(2, {0, 0, -9});
  s.enemies[0].zombie_flags |= 0x4000;
  push_zombie_grab_neighbors(s, 0, p, 0);
  ck(s.enemies[2].modes[2] == 3 && s.enemies[2].damage_velocity.z == -1,
     "Back grab knockback direction/mode wrong");
  s.enemies[2].modes[0] = 1;
  s.enemies[2].zombie_flags = 0;
  s.enemies[2].angles[1] = 32768;
  push_zombie_grab_neighbors(s, 0, p, 0);
  ck(s.enemies[2].modes[2] == 4 && s.enemies[2].zombie_flags == 0x2200,
     "Opposed neighbor facing must mark backward push");
  std::tie(s, p) = fixture();
  add(2, {0, 0, -9}, 26);
  s.enemies[0].zombie_flags |= 0x4000;
  push_zombie_grab_neighbors(s, 0, p, 0);
  ck(s.enemies[2].modes[2] == 2, "id26 must use its own push mode");
  std::tie(s, p) = fixture();
  add(2, {0, 0, -10});
  add(3, {0, 10, 0});
  push_zombie_grab_neighbors(s, 0, p, 0);
  ck(s.enemies[2].modes[0] == 1 && s.enemies[3].modes[0] == 1,
     "Push distance must be strict and three-dimensional");
  std::tie(s, p) = fixture();
  add(2, {0, 0, -100});
  add(3, {0, 0, -2});
  // seed2's first rand=59559187, %3=1: early return before distance/slot3.
  s.random_seed = 2;
  push_zombie_grab_neighbors(s, 0, p, 1);
  ck(s.random_seed == 2207042835u && s.enemies[3].modes[0] == 1,
     "Probabilistic push must return from scan before distance");
  std::tie(s, p) = fixture();
  add(2, {0, 0, -2});
  add(3, {NAN, 0, 0});
  bad([&] { push_zombie_grab_neighbors(s, 0, p, 0); });
  ck(s.enemies[2].modes[0] == 1 && s.enemies[1].modes[0] == 0 &&
         s.random_seed == 1,
     "Late neighbor failure leaked push changes");
  for (uint16_t type : {2, 3, 6, 7, 9, 10}) {
    std::tie(s, p) = fixture();
    add(2, {NAN, 0, 0});
    s.enemies[2].type = type;
    push_zombie_grab_neighbors(s, 0, p, 0);
    ck(s.enemies[2].modes[0] == 1, "Excluded neighbor type consumed geometry");
  }
  for (uint32_t flags : {0u, 3u, 0x81u}) {
    std::tie(s, p) = fixture();
    add(2, {NAN, 0, 0});
    s.enemies[2].flags = flags;
    push_zombie_grab_neighbors(s, 0, p, 0);
  }
  for (uint32_t flags : {0x400000u, 0x40000u, 0x200u}) {
    std::tie(s, p) = fixture();
    add(2, {NAN, 0, 0});
    s.enemies[2].zombie_flags = flags;
    push_zombie_grab_neighbors(s, 0, p, 0);
  }
}
void player_tests() {
  auto [s, p] = fixture();
  tick_zombie_player_grab(s, 0, p);
  ck(p.modes[3] == 1 && p.control0 == 1 && p.yaw_correction == 1 &&
         s.enemies[0].grab_yaw_offset == -5 && !(p.flags & 0x40000) &&
         p.player_work->status_flags == 0x1001e0,
     "Held grab startup fallthrough/alignment/status wrong");
  for (int i = 0; i < 4; ++i)
    tick_zombie_player_grab(s, 0, p);
  ck(s.enemies[0].grab_yaw_offset == -1 && p.control0 == 5,
     "Alignment must take five signed/truncated steps");
  tick_zombie_player_grab(s, 0, p);
  ck(s.enemies[0].grab_yaw_offset == 0 && p.control0 == 6,
     "Sixth alignment tick must snap exact source facing");
  const uint32_t offsets[] = {400, 425, 450, 425};
  const float held_z[] = {-6.326351f,  -6.326351f,  -5.f, -6.019392f,
                          -5.7556629f, -5.9214029f, -5.f, -6.019392f};
  for (uint8_t chr = 0; chr < 4; ++chr)
    for (bool back : {false, true}) {
      std::tie(s, p) = fixture();
      s.player_id = chr;
      s.enemies[0].zombie_flags |= back ? 0x4000 : 0;
      s.enemies[0].frame = 65536;
      tick_zombie_player_grab(s, 0, p);
      ck(p.motion_number == offsets[chr] + (back ? 6 : 7) &&
             p.motions == s.enemies[0].motions && p.frame == 0 &&
             p.blend_count == 5 && p.blend_rate == 42598 &&
             p.motion_mode == 0 && p.motion_step == 65536 &&
             s.enemies[0].zombie_work->action_anchor.z ==
                 held_z[chr * 2 + (back ? 0 : 1)] &&
             (s.enemies[0].zombie_flags & 0x80000),
         "Held character/facing clip mismatch");
      p.modes[3] = 2;
      p.motion_mode = 0x55;
      tick_zombie_player_grab(s, 0, p);
      ck(p.motion_number == offsets[chr] + (back ? 10 : 11) &&
             p.modes[3] == 3 && p.blend_count == 0 && p.blend_rate == 32768 &&
             p.motion_mode == 0x55 && (p.player_work->status_flags & 4) &&
             !(s.enemies[0].zombie_flags & 0x80000),
         "Escape transition loses motion mode or chooses wrong clip");
      p.frame = 39 * 65536;
      tick_zombie_player_grab(s, 0, p);
      ck(p.modes == std::array<uint8_t, 4>{1, 0, 0, 0} &&
             p.motions == p.base_motions && !(p.flags & 0x10004) &&
             (p.flags & 8) && !(p.state_flags & 0x10000) &&
             p.attack_flags == 0 && s.input_held == 0xabc0 &&
             !(s.enemies[0].zombie_flags & 0x24080),
         "Successful escape cleanup mismatch");
      std::tie(s, p) = fixture();
      s.player_id = chr;
      s.enemies[0].zombie_flags |= back ? 0x4000 : 0;
      p.modes[3] = 4;
      tick_zombie_player_grab(s, 0, p);
      ck(p.motion_number == offsets[chr] + (back ? 8 : 9) && p.modes[3] == 5 &&
             p.control0 == 1 &&
             s.pending_player_voices == std::vector<int32_t>{1026},
         "Lunge capture must immediately start held animation and voice");
      p.modes[3] = 6;
      tick_zombie_player_grab(s, 0, p);
      ck(p.motion_number == offsets[chr] + (back ? 12 : 13) &&
             p.modes[3] == 7 && !(p.flags & 0x10) &&
             s.pending_player_voices.back() == 1025,
         "Fatal grab chooses wrong clip/voice");
      p.frame = 39 * 65536;
      tick_zombie_player_grab(s, 0, p);
      ck(p.motion_step == 0 && (p.flags & 2) && !(p.flags & 0x10004) &&
             p.modes[0] == 4 && p.motions != p.base_motions &&
             s.input_held == 0xabc0 && !(s.enemies[0].zombie_flags & 0xa4080),
         "Fatal grab end cleanup mismatch");
    }
  std::tie(s, p) = fixture();
  p.modes[3] = 4;
  p.angles[1] = 0;
  s.enemies[0].zombie_flags |= 0x4000;
  tick_zombie_player_grab(s, 0, p);
  ck(p.yaw_correction == 6553 && s.enemies[0].grab_yaw_offset == 6553,
     "Exact180 alignment must stay positive then truncate toward zero");
  for (int i = 0; i < 5; ++i)
    tick_zombie_player_grab(s, 0, p);
  ck(s.enemies[0].grab_yaw_offset == -32768,
     "Signed16 offset snap lost exact180");
  std::tie(s, p) = fixture();
  p.modes[3] = 4;
  s.enemies[0].flags |= 0x10000;
  tick_zombie_player_grab(s, 0, p);
  ck(s.pending_player_voices.empty(),
     "Enemy primary10000 must suppress player voice");
  std::tie(s, p) = fixture();
  p.modes[3] = 1;
  s.enemies[0].frame = 12 * 65536;
  tick_zombie_player_grab(s, 0, p);
  ck(s.pending_player_voices == std::vector<int32_t>{1026},
     "Held frame12 voice absent");
  std::tie(s, p) = fixture();
  p.modes[3] = 8;
  tick_zombie_player_grab(s, 0, p);
  ck(p.modes == std::array<uint8_t, 4>{2, 1, 1, 0} && (p.flags & 4) &&
         p.motions == p.base_motions && (p.player_work->status_flags & 4),
     "Alternate grab release must restore original player locomotion modes");
  std::tie(s, p) = fixture();
  p.modes[3] = 2;
  p.motions = s.enemies[0].motions;
  p.base_motions.reset();
  p.frame = 39 * 65536;
  // Case2 falls through with freshly reset frame0: no release dependency yet.
  tick_zombie_player_grab(s, 0, p);
  p.frame = 39 * 65536;
  const auto before = p.flags;
  const auto zbefore = s.enemies[0].zombie_flags;
  bad([&] { tick_zombie_player_grab(s, 0, p); });
  ck(p.flags == before && s.enemies[0].zombie_flags == zbefore &&
         s.input_held == 0xabcd,
     "Missing release bank leaked flags or input");
  std::tie(s, p) = fixture();
  p.modes[3] = 4;
  p.player_work.reset();
  bad([&] { tick_zombie_player_grab(s, 0, p); });
  ck(p.modes[3] == 4 && s.pending_player_voices.empty() &&
         s.enemies[0].zombie_flags == 0x20000,
     "Missing player EXP1 leaked startup");
  std::tie(s, p) = fixture();
  p.modes[3] = 99;
  p.player_work.reset();
  s.player_id = 255;
  tick_zombie_player_grab(s, 0, p);
  ck(p.modes[3] == 99, "Unknown player submode consumed unused context");
  p.modes = {1, 0, 1, 0};
  tick_zombie_player_grab(s, 0, p);
  p.modes = {4, 0, 1, 0};
  s.enemies[0].zombie_flags = 0;
  tick_zombie_player_grab(s, 0, p);
  s.enemies[0].zombie_flags = 0x20000;
  bad([&] { tick_zombie_player_grab(s, 0, p); });
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--push")
      push_tests();
    else
      player_tests();
    std::cout << "Native player grab checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
