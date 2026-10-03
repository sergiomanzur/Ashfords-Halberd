#include "enemy_motion_internal.hpp"
#include "enemy_movement.hpp"
#include "enemy_targeting.hpp"
#include "enemy_wall.hpp"
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::bhEne01_MV01.
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
float finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite zombie walking context");
  return value;
}
} // namespace
void tick_zombie_walk(ScenarioState &state, size_t work,
                      const ScenarioEntity &player) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie walking work pool");
  auto &enemy = state.enemies[work];
  if (!(enemy.flags & 1) || enemy.id != 1 || !enemy.zombie_work)
    throw std::runtime_error("Missing zombie walking extended work");
  auto motion = detail::snapshot(enemy);
  auto flags = enemy.zombie_flags;
  auto modes = enemy.modes;
  auto timer = enemy.control0, timer1 = enemy.control1,
       sound_timer = enemy.control3;
  auto yaw = enemy.angles[1], previous_yaw = enemy.previous_angles[1];
  auto way = enemy.turn_speed;
  auto target_x = enemy.zombie_work->target_x,
       target_z = enemy.zombie_work->target_z;
  auto seed = state.random_seed;
  std::optional<size_t> body;
  std::optional<detail::MotionState> body_motion;
  uint32_t body_flags = 0;
  std::array<int32_t, 2> sounds{};
  size_t sound_count = 0;
  auto sound = [&] {
    // Both MV01 calls use low byte23: no active voice query or voice RNG.
    if (auto code = choose_zombie_sound(enemy, seed, 16847639, false)) {
      finite(enemy.position.x);
      finite(enemy.position.y);
      finite(enemy.position.z);
      sounds.at(sound_count++) = *code;
    }
  };
  auto target = [&] {
    target_x = finite(player.position.x);
    target_z = finite(player.position.z);
  };
  auto collision_body = [&] {
    if (!state.collision)
      throw std::runtime_error("Missing zombie walking wall context");
    CollisionBody result;
    result.position = enemy.position;
    result.flags = motion.flags;
    result.radius = enemy.geometry.radius;
    result.height = enemy.geometry.height;
    return result;
  };
  auto wall = [&] {
    const auto probe = collision_body();
    return enemy_direction_wall(*state.collision, state.dynamic_walls, probe,
                                yaw, 0, 6);
  };
  auto choice = [&] {
    if (!enemy.zombie_walk_choice)
      throw std::runtime_error("Missing original zombie walking choice");
    // This also validates the original 0/2/4 pair index for stop windows.
    zombie_walk_motion(*enemy.zombie_walk_choice, false);
    return *enemy.zombie_walk_choice;
  };
  switch (modes[3]) {
  case 0: {
    const bool special = enemy.type == 5;
    const auto index = special ? 0 : choice();
    detail::change(motion, special ? 125 : zombie_walk_motion(index, false), 0,
                   special ? 15 : 8);
    flags &= ~0x3000000u;
    body = enemy.zombie_work->body;
    if (body) {
      if (*body >= state.enemies.size() || *body == work)
        throw std::runtime_error("Missing zombie walking linked body");
      const auto &linked = state.enemies[*body];
      if (!(linked.flags & 1) || linked.id != 1 || !linked.zombie_work)
        throw std::runtime_error("Invalid zombie walking linked-body context");
      body_motion = detail::snapshot(linked);
      body_flags = linked.zombie_flags & ~0x3000000u;
      detail::change(*body_motion,
                     special ? 325 : zombie_walk_motion(index, true), 0,
                     special ? 15 : 8);
    }
    way = special ? 512 : 256;
    motion.flags |= 0x40000;
    timer = int32_t(random(seed) % 128) + 200;
    timer1 = 0;
    sound_timer = int32_t(random(seed) % 120);
    target();
    modes[3] = 1;
    [[fallthrough]];
  }
  case 1: {
    const float distance = finite(enemy.zombie_work->player_distance);
    if (!(motion.flags & 0x200000) &&
        distance < zombie_personality_at(enemy.zombie_work->personality)
                       .attack_distance &&
        ((flags & 0x400) || enemy.type == 5)) {
      modes[1] = 1;
      modes[2] = 2;
      modes[3] = 0;
      sound();
    } else if (distance <= 15) {
      modes[3] = 3;
      timer = int32_t(random(seed) % 64) + 100;
    } else if (wall()) {
      const auto hit = enemy_side_wall(*state.collision, state.dynamic_walls,
                                       collision_body(), yaw, 6, 0);
      if (!hit) {
        previous_yaw = uint16_t(
            uint32_t(enemy_direction(enemy.position, player.position)) -
            uint32_t(yaw));
        way = previous_yaw <= 32768 ? 256 : -256;
      } else
        way = int16_t(hit * 256);
      modes[3] = 2;
    } else {
      timer = add(timer, -1);
      if (timer < 0) {
        static constexpr int32_t windows[6][2] = {{13, 33}, {42, 59}, {10, 30},
                                                  {45, 59}, {10, 40}, {43, 77}};
        const auto index = choice();
        const auto frame = motion.frame / 65536;
        if (frame >= windows[index][0] && frame <= windows[index][1]) {
          motion.flags |= 0x80000;
          modes[1] = 0;
          modes[2] = 0;
          modes[3] = 0;
        } else if (frame >= windows[index + 1][0] &&
                   frame <= windows[index + 1][1]) {
          motion.flags &= ~0x80000u;
          modes[1] = 0;
          modes[2] = 0;
          modes[3] = 0;
        }
      }
    }
    break;
  }
  case 2:
    if (!wall())
      modes[3] = 1;
    else
      yaw = add(yaw, way);
    break;
  case 3:
    target();
    yaw = turn_enemy_yaw(enemy, {target_x, 0, target_z}, way);
    timer = add(timer, -1);
    if (timer < 0) {
      modes[3] = 1;
      timer = int32_t(random(seed) % 128) + 200;
    }
    break;
  }
  sound_timer = add(sound_timer, -1);
  if (sound_timer < 0) {
    sound();
    sound_timer = int32_t(random(seed) % 120) + 180;
  }
  if (sound_count)
    state.pending_enemy_sounds.reserve(state.pending_enemy_sounds.size() +
                                       sound_count);
  detail::commit(enemy, motion);
  enemy.zombie_flags = flags;
  enemy.modes = modes;
  enemy.control0 = timer;
  enemy.control1 = timer1;
  enemy.control3 = sound_timer;
  enemy.turn_speed = way;
  enemy.angles[1] = yaw;
  enemy.previous_angles[1] = previous_yaw;
  enemy.zombie_work->target_x = target_x;
  enemy.zombie_work->target_z = target_z;
  state.random_seed = seed;
  if (body_motion) {
    auto &linked = state.enemies[*body];
    detail::commit(linked, *body_motion);
    linked.zombie_flags = body_flags;
  }
  for (size_t i = 0; i < sound_count; ++i)
    state.pending_enemy_sounds.push_back({work, enemy.position, sounds[i]});
}
} // namespace ashford
