#include "enemy_motion_internal.hpp"
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::MV00/SePlay and ChgMtn.
namespace ashford {
namespace {
int32_t word(uint32_t value) {
  return value < 0x80000000u ? int32_t(value)
                             : int32_t(int64_t(value) - 0x100000000ll);
}
int32_t add(int32_t value, int32_t amount) {
  return word(uint32_t(value) + uint32_t(amount));
}
uint32_t random(uint32_t &seed) {
  seed = seed * 1103515245u + 12345u;
  return seed & 0x7fffffffu;
}
using detail::change;
using detail::commit;
using detail::MotionState;
using detail::snapshot;
} // namespace
int change_enemy_motion(ScenarioEntity &enemy, uint32_t number, int32_t frame,
                        int32_t rate) {
  return change(enemy, number, frame, rate);
}
std::optional<int32_t> choose_zombie_sound(const ScenarioEntity &enemy,
                                           uint32_t &seed, int32_t code,
                                           bool already_playing) {
  static constexpr int32_t offsets[27] = {28, 32, 16, 0, 4,  8,  32, 36, 0,
                                          8,  20, 24, 8, 12, 8,  12, 16, 4,
                                          40, 36, 40, 0, 8,  24, 20, 28, 8};
  if (enemy.flags & 0x10000)
    return std::nullopt;
  auto selected = code;
  auto next_seed = seed;
  if (uint8_t(code) >= 20 && uint8_t(code) < 64) {
    if (!enemy.zombie_work)
      throw std::runtime_error("Missing zombie sound personality");
    const auto personality = enemy.zombie_work->personality;
    zombie_personality_at(personality);
    selected = add(selected, offsets[personality]);
    if (uint8_t(code) == 20 || uint8_t(code) == 21) {
      if (already_playing)
        return std::nullopt;
      if (!(random(next_seed) % 2)) {
        seed = next_seed;
        return std::nullopt;
      }
    }
  }
  if (enemy.state_flags & 0x100000) {
    if (selected == 70414 || selected == 70415)
      selected = add(selected, 2);
    else if (selected == 8963) {
      seed = next_seed;
      return std::nullopt;
    }
  }
  seed = next_seed;
  return selected;
}
void tick_zombie_idle(ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie idle work pool");
  auto &enemy = state.enemies[work];
  if (!(enemy.flags & 1) || enemy.id != 1 || !enemy.zombie_work)
    throw std::runtime_error("Missing zombie idle extended work");
  auto motion = snapshot(enemy);
  auto flags = enemy.zombie_flags;
  auto modes = enemy.modes;
  auto timer = enemy.control0, sound_timer = enemy.control3;
  auto seed = state.random_seed;
  std::optional<size_t> body;
  std::optional<MotionState> body_motion;
  uint32_t body_flags = 0;
  int32_t body_neck = 0;
  if ((modes[3] == 0 && enemy.zombie_work->body) || modes[3] == 2 ||
      modes[3] == 3) {
    body = enemy.zombie_work->body;
    if (!body || *body >= state.enemies.size() || *body == work)
      throw std::runtime_error("Missing zombie idle linked-body work");
    const auto &linked = state.enemies[*body];
    if (!(linked.flags & 1) || linked.id != 1 || !linked.zombie_work)
      throw std::runtime_error("Invalid zombie idle linked-body context");
    body_motion = snapshot(linked);
    body_flags = linked.zombie_flags;
    body_neck = linked.zombie_work->neck_yaw;
  }
  switch (modes[3]) {
  case 0: {
    if (!enemy.motions || enemy.motion_number >= enemy.motions->size() ||
        !(*enemy.motions)[enemy.motion_number] ||
        !(*enemy.motions)[enemy.motion_number]->frames)
      throw std::runtime_error("Missing incoming zombie idle motion frames");
    const auto frames = (*enemy.motions)[enemy.motion_number]->frames;
    const auto frame = word((random(seed) % frames) * 65536u);
    change(motion, 2, 0, 15);
    flags &= ~0x3000000u;
    motion.frame = frame;
    if (body_motion) {
      change(*body_motion, 202, 0, 15);
      body_flags &= ~0x3000000u;
      body_motion->frame = frame;
    }
    motion.flags |= 0x40000;
    timer = int32_t(random(seed) % 10) * 20 + 15;
    flags = (flags & ~0x40fu) | 1;
    modes[1] = 1;
    sound_timer = int32_t(random(seed) % 110) + 10;
    ++modes[3];
    [[fallthrough]];
  }
  case 1:
    timer = add(timer, -1);
    if ((timer <= 0 || (flags & 0x400)) && enemy.type != 8) {
      modes[1] = 1;
      modes[2] = 1;
      modes[3] = 0;
    }
    break;
  case 2:
    body_neck = add(body_neck, 128);
    if (uint16_t(uint32_t(body_neck) - 9102u) < 47332) {
      body_neck = 9102;
      ++modes[3];
    }
    break;
  case 3:
    body_neck = add(body_neck, -128);
    if (uint16_t(uint32_t(body_neck) + 9102u) > 18204) {
      body_neck = 56434;
      modes[3] = 2;
    }
    break;
  }
  sound_timer = add(sound_timer, -1);
  std::optional<int32_t> sound;
  if (sound_timer < 0) {
    // This request has low byte23, so SePlay never queries active voice state.
    sound = choose_zombie_sound(enemy, seed, 16847639, false);
    sound_timer = int32_t(random(seed) % 120) + 180;
    if (sound) {
      if (!std::isfinite(enemy.position.x) ||
          !std::isfinite(enemy.position.y) || !std::isfinite(enemy.position.z))
        throw std::runtime_error("Nonfinite zombie idle sound origin");
      state.pending_enemy_sounds.reserve(state.pending_enemy_sounds.size() + 1);
    }
  }
  commit(enemy, motion);
  enemy.zombie_flags = flags;
  enemy.modes = modes;
  enemy.control0 = timer;
  enemy.control3 = sound_timer;
  state.random_seed = seed;
  if (body_motion) {
    auto &linked = state.enemies[*body];
    commit(linked, *body_motion);
    linked.zombie_flags = body_flags;
    linked.zombie_work->neck_yaw = body_neck;
  }
  if (sound)
    state.pending_enemy_sounds.push_back({work, enemy.position, *sound});
}
} // namespace ashford
