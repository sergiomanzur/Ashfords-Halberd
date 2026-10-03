#include "effects.hpp"
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
bool near(float a, float b) { return std::abs(a - b) < .00002f; }
template <class F> void bad(F fn) {
  bool failed = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Invalid consumed NG02 dependency accepted");
}
std::pair<ScenarioState, ScenarioEntity> fixture() {
  ScenarioState s;
  s.random_seed = 1;
  s.enemies.resize(128);
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t n : {9u, 209u, 35u, 235u, 36u, 236u}) {
    Motion m;
    m.frames = (n == 9 || n == 209) ? 50 : 40;
    bank->at(n) = m;
  }
  auto &e = s.enemies[0];
  e.flags = 0x61;
  e.id = 1;
  e.modes = {2, 0, 2, 0};
  e.motion_number = 85;
  e.motions = bank;
  e.zombie_flags = 0x300800f;
  e.zombie_work.emplace();
  e.zombie_work->body = 1;
  e.zombie_work->personality = 1;
  e.zombie_work->head_damage = 7;
  e.health = 75;
  e.control0 = 17;
  e.position = {1, 2, 3};
  e.damage_velocity = {7, 8, 9};
  e.model_work.emplace();
  e.model_work->joints.resize(11);
  e.model_work->joints[10].matrix[3] = 10;
  e.model_work->joints[10].matrix[7] = 20;
  e.model_work->joints[10].matrix[11] = 30;
  auto &b = s.enemies[1];
  b = e;
  b.flags = 0x81;
  b.zombie_flags = 0x830080af;
  b.motion_number = 285;
  b.zombie_work->body.reset();
  b.pose_owner = 0;
  b.model_work.reset();
  ScenarioEntity p;
  p.flags = 0x10004;
  p.modes = {1, 2, 3, 4};
  p.health = 100;
  p.control1 = 9;
  p.position = {10, 7, 20};
  return {s, p};
}
size_t effects(const ScenarioState &s) {
  size_t n = 0;
  if (s.effect_pool)
    for (const auto &w : s.effect_pool->works)
      n += bool(w.flags & 1);
  return n;
}
void starts_and_damage() {
  for (bool prior15 : {false, true})
    for (bool back : {false, true}) {
      auto [s, p] = fixture();
      auto &e = s.enemies[0];
      if (prior15)
        e.motion_number = 15;
      if (back)
        e.zombie_flags |= 0x4000;
      e.angles = {100, 16384, 200};
      tick_zombie_standing_grab(s, 0, p);
      ck(e.motion_number == 9 && s.enemies[1].motion_number == 209 &&
             e.frame == (prior15 ? 0 : 10) * 65536 &&
             s.enemies[1].frame == e.frame && e.blend_count == 10 &&
             s.enemies[1].blend_count == 10 && e.motion_mode == 0x20 &&
             e.motion_step == 65536 && e.flags == 0x21 &&
             e.secondary_flags == 1 && e.modes[3] == (prior15 ? 1 : 2) &&
             e.control0 == (prior15 ? 17 : 80) &&
             e.zombie_flags == (back ? 0x2c00fu : 0x2800fu) &&
             s.enemies[1].zombie_flags == 0x800080af &&
             p.modes == std::array<uint8_t, 4>{4, 0, 1, 0} &&
             near(e.position.x, 10 + (back ? 8.f : 8.844476f)) &&
             e.position.y == 7 && near(e.position.z, 20) &&
             e.angles == std::array<int32_t, 3>{100, 16384, 200} &&
             e.health == 75 && p.health == 100 && p.control1 == 9,
         "NG02 paired startup/fallthrough/position/source flags wrong");
    }
  auto [s, p] = fixture();
  auto &e = s.enemies[0];
  auto &b = s.enemies[1];
  e.motion_number = 9;
  e.frame = 10 * 65536;
  e.blend_count = 6;
  e.blend_rate = 300;
  e.motion_mode = 0x108;
  e.motion_step = 0;
  b.motion_number = 209;
  b.frame = 12 * 65536;
  b.blend_count = 9;
  b.blend_rate = 123;
  b.motion_mode = 0x66;
  tick_zombie_standing_grab(s, 0, p);
  ck(e.frame == 10 * 65536 && e.blend_count == 6 && e.blend_rate == 300 &&
         e.motion_mode == 0x108 && e.motion_step == 65536 && e.modes[3] == 2 &&
         b.frame == 12 * 65536 && b.blend_count == 9 && b.blend_rate == 123 &&
         b.motion_mode == 0x66 && b.motion_step == 65536,
     "NG02 same-motion must retain each work's counters");
  for (int frame : {9, 10, 11}) {
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].frame = frame * 65536;
    tick_zombie_standing_grab(s, 0, p);
    ck(s.enemies[0].modes[3] == (frame == 10 ? 2 : 1) &&
           s.enemies[0].control0 == (frame == 10 ? 80 : 17),
       "NG02 ct0 startup requires exact authored frame10");
  }
  for (int frame : {26, 27, 28}) {
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 2;
    s.enemies[0].frame = frame * 65536;
    tick_zombie_standing_grab(s, 0, p);
    ck(p.health == (frame == 27 ? 90 : 100) && s.enemies[0].control0 == 17 &&
           s.enemies[0].modes[3] == 2,
       "NG02 damage requires exact frame27 and does not decrement timer");
  }
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 2;
  s.enemies[0].frame = 27 * 65536;
  p.health = 3;
  tick_zombie_standing_grab(s, 0, p);
  ck(p.health == 0 && !p.state_flags,
     "NG02 clamps player HP at zero without NG00 fatal flags");
  tick_zombie_standing_grab(s, 0, p);
  ck(p.health == 0, "Repeated authored damage gate must keep zero clamp");
}
void releases_and_death() {
  for (uint8_t character : {uint8_t(0), uint8_t(1), uint8_t(3), uint8_t(255)})
    for (int trigger : {0, 1, 2}) {
      auto [s, p] = fixture();
      s.player_id = character;
      auto &e = s.enemies[0];
      e.modes[3] = 2;
      e.frame = 27 * 65536;
      e.control0 = trigger == 0 ? 0 : trigger == 1 ? -1 : 17;
      if (trigger == 2)
        e.flags |= 0x2000000;
      tick_zombie_standing_grab(s, 0, p);
      ck(e.motion_number == (character == 0 ? 36u : 35u) &&
             s.enemies[1].motion_number == (character == 0 ? 236u : 235u) &&
             e.frame == 0 && e.blend_count == 5 && e.blend_rate == 0 &&
             e.motion_mode == 0x20 && e.control0 == 0 && e.modes[3] == 3 &&
             (e.flags & 0x40000) && !(e.flags & 0x2000000) && p.modes[3] == 2 &&
             p.control1 == 5 && p.health == 90 && !s.effect_pool &&
             e.zombie_work->head_damage == 7,
         "NG02 release clip/player handoff must not fall through to death");
    }
  for (uint32_t motion : {36u, 35u}) {
    auto [s, p] = fixture();
    auto &e = s.enemies[0];
    e.modes[3] = 3;
    e.motion_number = motion;
    e.frame = (motion == 36 ? 24 : 33) * 65536;
    tick_zombie_standing_grab(s, 0, p);
    ck(effects(s) == (motion == 36 ? 8u : 4u) && e.health == 75 &&
           e.zombie_work->head_damage == 7 &&
           s.random_seed == (motion == 36 ? 1u : 3295386429u) &&
           e.damage_velocity.x == (motion == 36 ? 7.f : 0.f) &&
           e.damage_velocity.y == (motion == 36 ? 8.f : 0.f) &&
           e.damage_velocity.z == (motion == 36 ? 9.f : 0.f),
       "NG02 authored effect/velocity/RNG frame wrong");
    e.frame += 65536;
    tick_zombie_standing_grab(s, 0, p, {4877, 4869});
    ck(e.health == -1 && e.zombie_work->head_damage == -1 &&
           !(e.flags & 0x20) && s.enemies[1].health == 75 &&
           s.enemies[1].zombie_work->head_damage == 7 &&
           s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].code == (motion == 36 ? 4877 : 4869) &&
           s.pending_enemy_sounds[0].work == 0 &&
           s.pending_enemy_sounds[0].position.x == 1,
       "NG02 CutHead affects root only; head sound is not active-slot gated");
    if (motion == 35) {
      e.frame = 35 * 65536;
      tick_zombie_standing_grab(s, 0, p);
      ck(effects(s) == 12 && s.random_seed == 3295386429u,
         "NG02 non-Claire neck blood follows fragments at frame35");
    }
    e.frame = 38 * 65536;
    tick_zombie_standing_grab(s, 0, p);
    ck(e.modes[3] == 3 && e.motion_step == 65536,
       "NG02 terminal requires exact source frame count minus one");
    const auto body_flags = s.enemies[1].zombie_flags;
    e.frame = 39 * 65536;
    tick_zombie_standing_grab(s, 0, p);
    ck(e.motion_step == 0 && s.enemies[1].motion_step == 0 &&
           e.modes == std::array<uint8_t, 4>{4, 2, 0, 0} &&
           s.enemies[1].modes == e.modes && !(e.zombie_flags & 0x800f) &&
           s.enemies[1].zombie_flags == body_flags,
       "NG02 paired death stop clears root flags only");
  }
  auto [s, p] = fixture();
  s.enemies[0].modes[3] = 3;
  s.enemies[0].motion_number = 36;
  s.enemies[0].frame = 25 * 65536;
  s.enemies[0].flags |= 0x10000;
  tick_zombie_standing_grab(s, 0, p);
  ck(s.pending_enemy_sounds.empty() && s.enemies[0].health == -1,
     "Muted source head sound must not suppress death");
}
void failures_and_unused_context() {
  auto [s, p] = fixture();
  s.enemies[0].zombie_work->body.reset();
  s.enemies[1].flags = 0;
  tick_zombie_standing_grab(s, 0, p);
  ck(s.enemies[0].motion_number == 9 && s.enemies[1].motion_number == 285,
     "NG02 null body must skip paired work");
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 99;
  s.enemies[0].zombie_work->body = 999;
  s.enemies[0].zombie_work->personality = 255;
  s.enemies[0].motions.reset();
  s.enemies[0].model_work.reset();
  tick_zombie_standing_grab(s, 0, p);
  ck(s.enemies[0].modes[3] == 99 && p.health == 100 && !s.effect_pool,
     "NG02 unknown state must ignore unused body/personality/motion/cache");
  for (size_t invalid : {size_t(0), size_t(128)}) {
    std::tie(s, p) = fixture();
    s.enemies[0].zombie_work->body = invalid;
    bad([&] { tick_zombie_standing_grab(s, 0, p); });
    ck(s.enemies[0].motion_number == 85 && s.enemies[0].flags == 0x61 &&
           p.modes == std::array<uint8_t, 4>{1, 2, 3, 4},
       "NG02 late paired startup error must roll back root/player");
  }
  std::tie(s, p) = fixture();
  bad([&] { tick_zombie_standing_grab(s, 0, s.enemies[1]); });
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 2;
  s.enemies[0].frame = 27 * 65536;
  s.enemies[0].zombie_work->personality = 255;
  bad([&] { tick_zombie_standing_grab(s, 0, p); });
  ck(p.health == 100 && s.enemies[0].control0 == 17,
     "NG02 consumed personality failure must retain player HP");
  for (uint32_t motion : {36u, 35u}) {
    std::tie(s, p) = fixture();
    s.enemies[0].modes[3] = 3;
    s.enemies[0].motion_number = motion;
    s.enemies[0].frame = (motion == 36 ? 24 : 33) * 65536;
    s.enemies[0].motions.reset();
    auto pool = std::make_shared<NativeEffectPool>();
    pool->scratch.id = 77;
    s.effect_pool = pool;
    bad([&] { tick_zombie_standing_grab(s, 0, p); });
    ck(s.effect_pool == pool && s.effect_pool->scratch.id == 77 &&
           effects(s) == 0 && s.random_seed == 1 &&
           s.enemies[0].damage_velocity.x == 7 &&
           s.enemies[0].zombie_work->head_damage == 7 && p.health == 100,
       "NG02 late bank failure must restore staged effects/RNG/velocity");
  }
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 3;
  s.enemies[0].motion_number = 36;
  s.enemies[0].frame = 25 * 65536;
  s.enemies[0].motions.reset();
  bad([&] { tick_zombie_standing_grab(s, 0, p); });
  ck(s.enemies[0].health == 75 && s.enemies[0].zombie_work->head_damage == 7 &&
         s.pending_enemy_sounds.empty(),
     "NG02 late bank failure must roll back head damage/HP/sound request");
  std::tie(s, p) = fixture();
  s.enemies[0].modes[3] = 3;
  s.enemies[0].motion_number = 35;
  s.enemies[0].frame = 33 * 65536;
  s.enemies[0].model_work->joints[10].matrix[0] =
      std::numeric_limits<double>::quiet_NaN();
  bad([&] { tick_zombie_standing_grab(s, 0, p); });
  ck(!s.effect_pool && s.random_seed == 1 &&
         s.enemies[0].damage_velocity.x == 7,
     "NG02 invalid fragment joint must preserve staged zero velocity");
}
} // namespace
int main() {
  try {
    starts_and_damage();
    releases_and_death();
    failures_and_unused_context();
    std::cout << "Native source NG02 standing-grab checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
