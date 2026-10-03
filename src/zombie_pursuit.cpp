#include "enemy_motion_internal.hpp"
#include "enemy_movement.hpp"
#include "enemy_wall.hpp"
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
// Native en01.c MV02, ChgWalkMtn and FastWalkCheck; scalar staging only.
namespace ashford {
namespace {
int32_t word(uint32_t v) {
  return v < 0x80000000u ? int32_t(v) : int32_t(int64_t(v) - 0x100000000ll);
}
int32_t add(int32_t v, int32_t amount) {
  return word(uint32_t(v) + uint32_t(amount));
}
uint32_t random(uint32_t &seed) {
  seed = seed * 1103515245u + 12345u;
  return seed & 0x7fffffffu;
}
float finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite close-pursuit context");
  return value;
}
const ScenarioEntity &root(const ScenarioState &s, size_t work) {
  if (s.enemies.size() != 128 || work >= s.enemies.size() ||
      !(s.enemies[work].flags & 1) || s.enemies[work].id != 1 ||
      !s.enemies[work].zombie_work)
    throw std::runtime_error("Invalid close-pursuit work");
  return s.enemies[work];
}
struct Pursuit {
  const ScenarioState &state;
  const ScenarioEntity &enemy;
  size_t work;
  detail::MotionState motion;
  uint32_t flags, secondary;
  std::array<uint8_t, 4> modes;
  std::array<int32_t, 4> timers;
  int32_t yaw, phase, transition;
  int16_t way;
  int8_t delay;
  std::optional<size_t> body;
  std::optional<detail::MotionState> body_motion;
  uint32_t body_flags = 0;
  explicit Pursuit(const ScenarioState &s, size_t index)
      : state(s), enemy(root(s, index)), work(index),
        motion(detail::snapshot(enemy)), flags(enemy.zombie_flags),
        secondary(enemy.zombie_secondary_flags), modes(enemy.modes),
        timers{enemy.control0, enemy.control1, enemy.control2, enemy.control3},
        yaw(enemy.angles[1]), phase(enemy.zombie_work->pursuit_phase_word),
        transition(enemy.zombie_work->pursuit_transition_timer),
        way(enemy.turn_speed), delay(enemy.zombie_work->attack_delay) {}
  void link_body(bool required) {
    if (body_motion)
      return;
    body = enemy.zombie_work->body;
    if (!body) {
      if (required)
        throw std::runtime_error("Walking transition requires linked body");
      return;
    }
    if (*body == work)
      throw std::runtime_error("Close-pursuit body aliases root");
    const auto &linked = root(state, *body);
    body_motion = detail::snapshot(linked);
    body_flags = linked.zombie_flags;
  }
  int32_t choice() const {
    if (!enemy.zombie_walk_choice)
      throw std::runtime_error("Missing pursuit walk pair");
    zombie_walk_motion(*enemy.zombie_walk_choice, false);
    return *enemy.zombie_walk_choice;
  }
  uint32_t frames(uint32_t number) const {
    if (!enemy.motions || number >= enemy.motions->size() ||
        !enemy.motions->at(number) || !enemy.motions->at(number)->frames ||
        enemy.motions->at(number)->frames > 65535)
      throw std::runtime_error("Missing walking phase motion frames");
    return enemy.motions->at(number)->frames;
  }
  int32_t mapped_frame(uint32_t number) const {
    const auto old_count = frames(motion.motion_number),
               new_count = frames(number);
    const float phase_value =
        finite(float(new_count) *
               ((100.f / float(old_count)) * float(motion.frame / 65536)));
    const float integer = finite(phase_value / 100.f);
    if (double(integer) < -2147483648. || double(integer) >= 2147483648.)
      throw std::runtime_error("Walking phase outside source integer range");
    auto frame = word(uint32_t(int32_t(integer)) * 65536u);
    if (frame < 0)
      frame = 0;
    if (uint32_t(frame) > new_count)
      frame = int32_t(new_count);
    // USA0x1867a8 shifts the helper's already clamped result a second time.
    return word(uint32_t(frame) * 65536u);
  }
  void change_walk() {
    link_body(true);
    uint32_t main_number, body_number;
    const auto number = motion.motion_number,
               linked = body_motion->motion_number;
    if (number == 117 || number == 125) {
      if (linked == (number == 117 ? 317u : 325u)) {
        main_number = zombie_walk_motion(choice(), false);
        body_number = 119;
      } else {
        main_number = number;
        body_number = number == 117 ? 317 : 325;
      }
    } else if (linked == 200 || linked == 240 || linked == 241) {
      main_number = secondary & 0x80 ? 125 : 117;
      body_number = 118;
    } else {
      const auto pair = choice();
      main_number = zombie_walk_motion(pair, false);
      body_number = zombie_walk_motion(pair, true);
    }
    const auto frame = mapped_frame(main_number);
    detail::change(motion, main_number, frame, 20);
    flags &= ~0x3000000u;
    detail::change(*body_motion, body_number,
                   body_number == 118 || body_number == 119 ? 0 : motion.frame,
                   10);
    body_flags &= ~0x3000000u;
    motion.flags |= 0x40000;
  }
  ScenarioEntity probe() const {
    ScenarioEntity result;
    result.flags = motion.flags;
    result.position = enemy.position;
    result.angles[1] = yaw;
    return result;
  }
  void turn(int32_t step) {
    yaw = turn_enemy_yaw(
        probe(), {enemy.zombie_work->target_x, 0, enemy.zombie_work->target_z},
        step);
  }
  float distance() const { return finite(enemy.zombie_work->player_distance); }
  void commit(ScenarioState &s) const {
    auto &e = s.enemies[work];
    detail::commit(e, motion);
    e.zombie_flags = flags;
    e.zombie_secondary_flags = secondary;
    e.modes = modes;
    e.control0 = timers[0];
    e.control1 = timers[1];
    e.control2 = timers[2];
    e.control3 = timers[3];
    e.angles[1] = yaw;
    e.turn_speed = way;
    e.zombie_work->pursuit_phase_word = phase;
    e.zombie_work->pursuit_transition_timer = transition;
    e.zombie_work->attack_delay = delay;
    if (body_motion) {
      detail::commit(s.enemies[*body], *body_motion);
      s.enemies[*body].zombie_flags = body_flags;
    }
  }
};
} // namespace
bool zombie_fast_walk_active(const ScenarioState &s, size_t work) {
  root(s, work);
  for (size_t i = 0; i < s.enemies.size(); ++i) {
    const auto &e = s.enemies[i];
    if (e.id == 1 && e.type != 5 && (e.flags & 1) && !(e.flags & 0x82) &&
        e.zombie_work && i != work && (e.zombie_secondary_flags & 0x80))
      return true;
  }
  return false;
}
void change_zombie_walk_motion(ScenarioState &s, size_t work) {
  Pursuit p(s, work);
  p.change_walk();
  p.commit(s);
}
void tick_zombie_close_pursuit(ScenarioState &s, size_t work,
                               const ScenarioEntity &player) {
  Pursuit p(s, work);
  auto seed = s.random_seed;
  std::array<int32_t, 2> sounds{};
  size_t sound_count = 0;
  auto sound = [&] {
    if (auto code = choose_zombie_sound(p.enemy, seed, 16847639, false)) {
      finite(p.enemy.position.x);
      finite(p.enemy.position.y);
      finite(p.enemy.position.z);
      sounds.at(sound_count++) = *code;
    }
  };
  switch (p.modes[3]) {
  case 0: {
    const auto pair = p.choice();
    detail::change(p.motion, zombie_walk_motion(pair, false), 0, 8);
    p.flags &= ~0x3000000u;
    p.link_body(false);
    if (p.body_motion) {
      detail::change(*p.body_motion, zombie_walk_motion(pair, true), 0, 8);
      p.body_flags &= ~0x3000000u;
    }
    p.way = 256;
    p.motion.flags |= 0x40000;
    p.timers[0] = p.timers[1] = 0;
    p.timers[2] = int32_t(random(seed) % 120) + 120;
    p.phase = 0;
    p.transition = 0;
    p.timers[3] = int32_t(random(seed) % 120);
    p.secondary &= ~0x80u;
    p.flags = (p.flags & ~0xfu) | 2;
    ++p.modes[3];
    [[fallthrough]];
  }
  case 1: {
    if (p.enemy.type == 5)
      p.turn(p.way);
    else if (!(p.secondary & 0x80) && p.distance() >= 15) {
      if (random(seed) % 3) {
        p.turn(p.way);
        p.timers[1] = add(p.timers[1], 1);
        const auto end = add(p.timers[2], 30);
        if (p.timers[1] > p.timers[2] && p.timers[1] < end)
          p.turn(-2 * int32_t(p.way));
        if (p.timers[1] > end) {
          p.timers[2] = int32_t(random(seed) % 120) + 120;
          p.timers[1] = 0;
        }
      }
    } else
      p.turn(p.way);
    if (p.transition == 0) {
      if (p.phase == 0) {
        const bool near = p.distance() <= (p.secondary & 2 ? 25.f : 20.f);
        if ((near && p.enemy.floor == player.floor &&
             !p.enemy.zombie_work->path_blocked &&
             enemy_turn_step(p.probe(), player.position, 16384) == 0) ||
            p.enemy.type == 5) {
          sound();
          if (p.enemy.type == 5)
            p.secondary |= 0x80;
          else
            p.secondary &= ~0x80u;
          p.timers[3] = int32_t(random(seed) % 120) + 180;
          p.change_walk();
          p.delay = 5;
          p.way = 512;
          p.timers[0] = int32_t(random(seed) % 60) + 30;
          p.phase = 1;
          p.transition = 18;
        }
      } else if (p.phase == 1) {
        if (!(s.flags[5][0] & 0x80000000u) && random(seed) % 10 == 1 &&
            (p.motion.flags & 0x2000000) && p.motion.motion_number == 117 &&
            !zombie_fast_walk_active(s, work)) {
          p.secondary |= 0x80;
          detail::change(p.motion, 125, 0, 3);
          p.flags &= ~0x3000000u;
          p.link_body(false);
          if (p.body_motion) {
            detail::change(*p.body_motion, 325, 0, 3);
            p.body_flags &= ~0x3000000u;
          }
          p.motion.flags |= 0x40000;
        }
        if (p.enemy.type != 5 &&
            (p.distance() > (p.secondary & 2 ? 30.f : 25.f) ||
             p.enemy.floor != player.floor)) {
          p.timers[0] = add(p.timers[0], -1);
          if (p.timers[0] < 0) {
            p.change_walk();
            p.way = 256;
            p.motion.flags |= 0x40000;
            p.secondary &= ~0x80u;
            p.phase = 0;
            p.transition = 18;
          }
        }
      }
    }
    if (p.motion.flags & 0x200000) {
      p.modes[2] = 1;
      p.modes[3] = 0;
    }
    if (p.enemy.type != 5) {
      if (!(p.distance() >
            zombie_personality_at(p.enemy.zombie_work->personality)
                .notice_distance) &&
          (p.flags & 0x400))
        break;
      if (p.motion.flags & 0x2000000) {
        p.modes[2] = 1;
        p.modes[3] = 0;
      }
    }
    break;
  }
  case 2: {
    if (!s.collision)
      throw std::runtime_error("Pursuit wall recovery requires collision");
    CollisionBody body;
    body.position = p.enemy.position;
    body.flags = p.motion.flags;
    body.radius = p.enemy.geometry.radius;
    body.height = p.enemy.geometry.height;
    if (!enemy_direction_wall(*s.collision, s.dynamic_walls, body, p.yaw, 0, 6))
      p.modes[3] = 1;
    else
      p.yaw = add(p.yaw, p.way);
    break;
  }
  }
  if (p.transition > 0 && --p.transition == 0)
    p.change_walk();
  p.timers[3] = add(p.timers[3], -1);
  if (p.timers[3] < 0) {
    sound();
    p.timers[3] = int32_t(random(seed) % 120) + 180;
  }
  s.pending_enemy_sounds.reserve(s.pending_enemy_sounds.size() + sound_count);
  p.commit(s);
  s.random_seed = seed;
  for (size_t i = 0; i < sound_count; ++i)
    s.pending_enemy_sounds.push_back({work, p.enemy.position, sounds[i]});
}
} // namespace ashford
