#include "enemy_motion_internal.hpp"
#include "enemy_movement.hpp"
#include "enemy_targeting.hpp"
#include "zombie.hpp"
#include "zombie_attack_internal.hpp"
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::MV06/MV07. The caller
// performs surrounding Move/PlayerControl/SetMtn/CollCheck/CalcEnemy dispatch.
namespace ashford {
namespace {
int32_t word(uint32_t value) {
  return value < 0x80000000u ? int32_t(value)
                             : int32_t(int64_t(value) - 0x100000000ll);
}
const ScenarioEntity &work(const ScenarioState &s, size_t slot) {
  if (s.enemies.size() != 128 || slot >= s.enemies.size() ||
      !(s.enemies[slot].flags & 1) || s.enemies[slot].id != 1 ||
      !s.enemies[slot].zombie_work)
    throw std::runtime_error("Invalid zombie retry/turn work");
  return s.enemies[slot];
}
struct Staged {
  ScenarioState &state;
  size_t slot;
  const ScenarioEntity &enemy;
  detail::MotionState motion;
  uint32_t flags, seed;
  std::array<uint8_t, 4> modes;
  int32_t yaw, increment, timer;
  bool has_body = false;
  size_t body = 0;
  detail::MotionState body_motion{};
  uint32_t body_flags = 0;
  std::array<uint8_t, 4> body_modes{};
  Staged(ScenarioState &s, size_t index, const ScenarioEntity &player)
      : state(s), slot(index), enemy(work(s, index)),
        motion(detail::snapshot(enemy)), flags(enemy.zombie_flags),
        seed(s.random_seed), modes(enemy.modes), yaw(enemy.angles[1]),
        increment(enemy.yaw_correction), timer(enemy.control0) {
    for (const auto &e : s.enemies)
      if (&e == &player)
        throw std::runtime_error("Player aliases zombie retry/turn pool");
  }
  void link() {
    if (has_body || !enemy.zombie_work->body)
      return;
    body = *enemy.zombie_work->body;
    if (body == slot)
      throw std::runtime_error("Retry/turn body aliases root");
    const auto &b = work(state, body);
    body_motion = detail::snapshot(b);
    body_flags = b.zombie_flags;
    body_modes = b.modes;
    has_body = true;
  }
  void change(uint32_t main, uint32_t paired, int32_t frame, int32_t blend) {
    detail::change(motion, main, frame, blend);
    flags &= ~0x3000000u;
    link();
    if (has_body) {
      detail::change(body_motion, paired, frame, blend);
      body_flags &= ~0x3000000u;
    }
  }
  void commit() {
    auto &e = state.enemies[slot];
    detail::commit(e, motion);
    e.zombie_flags = flags;
    e.modes = modes;
    e.angles[1] = yaw;
    e.yaw_correction = increment;
    e.control0 = timer;
    state.random_seed = seed;
    if (has_body) {
      auto &b = state.enemies[body];
      detail::commit(b, body_motion);
      b.zombie_flags = body_flags;
      b.modes = body_modes;
    }
  }
};
} // namespace
void tick_zombie_capture_retry(ScenarioState &s, size_t slot,
                               ScenarioEntity &player) {
  Staged p(s, slot, player);
  auto player_flags = player.flags;
  switch (p.modes[3]) {
  case 0:
    p.change(49, 249, 2949120, 15);
    p.motion.flags |= 0x40000;
    p.seed = p.seed * 1103515245u + 12345u;
    p.timer = int32_t((p.seed & 0x7fffffffu) % 15) + 15;
    p.modes[1] = 1;
    ++p.modes[3];
    [[fallthrough]];
  case 1: {
    ScenarioEntity probe;
    probe.position = p.enemy.position;
    probe.angles[1] = p.yaw;
    probe.flags = p.motion.flags;
    p.yaw = word(uint32_t(p.yaw) +
                 uint32_t(enemy_turn_step(probe, player.position, 910)));
    if (!(player.flags & 4)) {
      if (detail::zombie_capture_yaw(s, slot, player, p.yaw, 4551, 5.5f, 0)) {
        p.modes = {2, 0, 0, 0};
        p.link();
        if (p.has_body)
          p.body_modes = p.modes;
        player_flags |= 0x10004;
        p.flags |= 0x80;
        break;
      }
      p.modes[1] = 0;
      p.modes[2] = 5;
      p.modes[3] = 0;
    }
    break;
  }
  }
  p.commit();
  player.flags = player_flags;
}
void tick_zombie_turn(ScenarioState &s, size_t slot,
                      const ScenarioEntity &player) {
  Staged p(s, slot, player);
  switch (p.modes[3]) {
  case 0: {
    p.change(127, 327, 0, 8);
    const auto delta =
        uint16_t(uint32_t(enemy_direction(p.enemy.position, player.position)) -
                 uint32_t(p.yaw));
    if (delta < 0x8001)
      p.increment = delta / 22;
    else {
      p.increment = -(0x10000 - int32_t(delta)) / 22;
      p.motion.motion_mode |= 2;
      if (p.has_body)
        p.body_motion.motion_mode |= 2;
    }
    p.motion.flags |= 0x40000;
    ++p.modes[3];
    [[fallthrough]];
  }
  case 1: {
    const auto frame = p.motion.frame / 65536;
    if (frame >= 17 && frame < 40)
      p.yaw = word(uint32_t(p.yaw) + uint32_t(p.increment));
    if (frame == 40) {
      p.modes[1] = 1;
      p.modes[2] = 1;
      p.modes[3] = 0;
    }
    break;
  }
  }
  p.commit();
}
} // namespace ashford
