#include "enemy_motion_internal.hpp"
#include "enemy_navigation_internal.hpp"
#include "zombie.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::MV08. Source room target
// and saved kdnp reservation remain separate; SetMtn/CalcEnemy run afterward.
namespace ashford {
namespace {
int32_t word(uint32_t v) {
  return v < 0x80000000u ? int32_t(v) : int32_t(int64_t(v) - 0x100000000ll);
}
int8_t byte(uint32_t v) {
  v &= 255u;
  return int8_t(v < 128 ? int(v) : int(v) - 256);
}
float finite(float v) {
  if (!std::isfinite(v))
    throw std::runtime_error("Nonfinite zombie stair value");
  return v;
}
Vec3 vector(Vec3 v) {
  finite(v.x);
  finite(v.y);
  finite(v.z);
  return v;
}
const ScenarioEntity &work(const ScenarioState &s, size_t slot) {
  if (s.enemies.size() != 128 || slot >= s.enemies.size() ||
      !(s.enemies[slot].flags & 1) || s.enemies[slot].id != 1 ||
      !s.enemies[slot].zombie_work)
    throw std::runtime_error("Invalid zombie stair work");
  return s.enemies[slot];
}
struct Staged {
  ScenarioState &state;
  size_t slot;
  const ScenarioEntity &enemy;
  detail::MotionState motion;
  uint32_t flags, secondary, secondary_flags;
  std::array<uint8_t, 4> modes;
  Vec3 position, previous, anchor;
  int32_t yaw, target_yaw, increment, timer, remaining;
  float target_height, target_x, target_z;
  int16_t turn;
  int8_t floor, start_floor, step;
  bool has_body = false;
  size_t body = 0;
  detail::MotionState body_motion{};
  uint32_t body_flags = 0;
  Vec3 *reset_pose = nullptr;
  std::optional<detail::StairPair> release;
  Staged(ScenarioState &s, size_t i)
      : state(s), slot(i), enemy(work(s, i)), motion(detail::snapshot(enemy)),
        flags(enemy.zombie_flags), secondary(enemy.zombie_secondary_flags),
        secondary_flags(enemy.secondary_flags), modes(enemy.modes),
        position(enemy.position), previous(enemy.previous_position),
        anchor(enemy.zombie_work->action_anchor), yaw(enemy.angles[1]),
        target_yaw(enemy.pitch_correction), increment(enemy.yaw_correction),
        timer(enemy.control0), remaining(enemy.control1),
        target_height(enemy.floor_target_height),
        target_x(enemy.zombie_work->target_x),
        target_z(enemy.zombie_work->target_z), turn(enemy.turn_speed),
        floor(enemy.floor), start_floor(enemy.zombie_work->stair_start_floor),
        step(enemy.zombie_work->stair_step) {}
  void link() {
    if (has_body || !enemy.zombie_work->body)
      return;
    body = *enemy.zombie_work->body;
    if (body == slot)
      throw std::runtime_error("Stair body aliases root");
    const auto &b = work(state, body);
    body_motion = detail::snapshot(b);
    body_flags = b.zombie_flags;
    has_body = true;
  }
  void change(uint32_t main, uint32_t paired, int32_t frame, int32_t blend,
              bool translating = false) {
    detail::change(motion, main, frame, blend);
    flags &= ~(translating ? 0x2000000u : 0x3000000u);
    if (translating)
      flags |= 0x1000000;
    link();
    if (has_body) {
      detail::change(body_motion, paired, frame, blend);
      body_flags &= ~(translating ? 0x2000000u : 0x3000000u);
      if (translating)
        body_flags |= 0x1000000;
    }
  }
  ScenarioEntity &owner() {
    const size_t i = enemy.pose_owner.value_or(slot);
    if (i >= state.enemies.size() || !(state.enemies[i].flags & 1) ||
        (i != slot && state.enemies[i].pose_owner))
      throw std::runtime_error("Invalid stair model owner");
    return state.enemies[i];
  }
  float cached_y() {
    const auto &o = owner();
    if (!o.model_work || o.model_work->joints.empty())
      throw std::runtime_error("Missing stair cached root joint");
    const double y = o.model_work->joints[0].matrix[7];
    if (!std::isfinite(y) || std::abs(y) > std::numeric_limits<float>::max())
      throw std::runtime_error("Stair cached Y outside source float range");
    return float(y);
  }
  void advance_step() {
    step = byte(uint32_t(int32_t(step)) +
                ((flags & 0x800000) ? 1u : uint32_t(-1)));
  }
  void height_step() {
    position.y =
        finite(((flags & 0x800000) ? 1.493898f : .880405f) + cached_y());
    advance_step();
  }
  void remember() { anchor = vector(position); }
  void commit() {
    auto &e = state.enemies[slot];
    detail::commit(e, motion);
    e.zombie_flags = flags;
    e.zombie_secondary_flags = secondary;
    e.secondary_flags = secondary_flags;
    e.modes = modes;
    e.position = position;
    e.previous_position = previous;
    e.angles[1] = yaw;
    e.pitch_correction = target_yaw;
    e.yaw_correction = increment;
    e.control0 = timer;
    e.control1 = remaining;
    e.floor_target_height = target_height;
    e.turn_speed = turn;
    e.floor = floor;
    e.zombie_work->target_x = target_x;
    e.zombie_work->target_z = target_z;
    e.zombie_work->action_anchor = anchor;
    e.zombie_work->stair_start_floor = start_floor;
    e.zombie_work->stair_step = step;
    if (has_body) {
      detail::commit(state.enemies[body], body_motion);
      state.enemies[body].zombie_flags = body_flags;
    }
    if (reset_pose)
      reset_pose->y = 0;
    if (release) {
      release->first->attribute &= ~0x400000u;
      release->second->attribute &= ~0x400000u;
    }
  }
};
} // namespace
void tick_zombie_stairs(ScenarioState &s, size_t slot,
                        const ScenarioEntity &player) {
  Staged p(s, slot);
  switch (p.modes[3]) {
  case 0: {
    p.change(2, 202, 0, 10);
    if (!s.collision || !p.enemy.zombie_work->stair_target)
      throw std::runtime_error("Missing zombie stair target");
    const auto index = *p.enemy.zombie_work->stair_target;
    const auto &records = s.collision->miscellaneous;
    const size_t dynamic = index >= records.size() ? index - records.size() : 0;
    if (index >= records.size() && dynamic >= s.dynamic_miscellaneous.size())
      throw std::runtime_error("Stair target outside merged records");
    const auto &r = index < records.size() ? records[index]
                                           : s.dynamic_miscellaneous[dynamic];
    const bool up = r.parameters[0] == 0;
    if (up)
      p.flags |= 0x800000;
    else
      p.flags &= ~0x800000u;
    p.step = up ? 0 : byte(r.parameters[2]);
    const float amount = int(r.parameters[2]) * 2;
    p.target_height = finite(up ? finite(p.position.y) + amount
                                : finite(p.position.y) - amount);
    p.start_floor = p.floor;
    p.flags |= 0x400000;
    p.secondary |= 8;
    p.secondary_flags |= 1;
    static constexpr int32_t angles[] = {0, 49152, 32768, 16384};
    if (r.parameters[1] >= 4)
      throw std::runtime_error("Stair direction outside original yaw table");
    p.target_yaw = angles[r.parameters[1]];
    p.increment = uint16_t(uint32_t(p.target_yaw) - uint32_t(p.yaw));
    if (p.increment > 32768)
      p.increment -= 65536;
    p.increment /= 10;
    p.flags &= ~0xfu;
    p.turn = 256;
    p.remaining = int(r.parameters[2]) - 2;
    p.timer = 10;
    p.remember();
    ++p.modes[3];
    [[fallthrough]];
  }
  case 1:
    p.timer = word(uint32_t(p.timer) - 1u);
    if (p.timer >= 0)
      p.yaw = word(uint32_t(p.yaw) + uint32_t(p.increment));
    p.target_x = finite(player.position.x);
    p.target_z = finite(player.position.z);
    if (!p.motion.blend_count) {
      p.change((p.flags & 0x800000) ? 58 : 59, (p.flags & 0x800000) ? 258 : 259,
               0, 10, true);
      p.yaw = p.target_yaw;
      p.remember();
      ++p.modes[3];
    }
    break;
  case 2:
    if (p.motion.flags & 0x2000000) {
      p.height_step();
      p.change((p.flags & 0x800000) ? 56 : 57, (p.flags & 0x800000) ? 256 : 257,
               65536, 0, true);
      p.remember();
      ++p.modes[3];
    }
    break;
  case 3:
    if (p.motion.frame / 65536 == 40)
      p.advance_step();
    if (p.motion.flags & 0x2000000) {
      p.remaining = word(uint32_t(p.remaining) - 2u);
      if (p.remaining != 0) {
        p.motion.frame = 65536;
        p.height_step();
        p.remember();
        break;
      }
      p.height_step();
      p.change((p.flags & 0x800000) ? 60 : 61, (p.flags & 0x800000) ? 260 : 261,
               65536, 0, true);
      p.remember();
      ++p.modes[3];
    }
    break;
  case 4:
    if (p.motion.flags & 0x2000000) {
      p.remember();
      p.previous = p.position;
      p.advance_step();
      if (!s.collision)
        throw std::runtime_error("Missing stair destination ground");
      p.floor = player_floor_number(*s.collision, p.target_height);
      p.position.y = finite(s.collision->ground[size_t(int(p.floor) + 2)]);
      auto &o = p.owner();
      if (o.joint_pose.empty() || !o.joint_pose[0].position)
        throw std::runtime_error("Missing stair owner root position");
      p.reset_pose = &*o.joint_pose[0].position;
      if (!p.enemy.zombie_walk_choice)
        throw std::runtime_error("Missing stair walking motion choice");
      const auto choice = *p.enemy.zombie_walk_choice;
      const auto main = zombie_walk_motion(choice, false);
      // The linked walking table is consumed only when a body exists.
      const auto paired =
          p.enemy.zombie_work->body ? zombie_walk_motion(choice, true) : 0;
      p.change(main, paired, 3276800, 15);
      p.motion.flags |= 0x40000;
      p.secondary_flags &= ~1u;
      if (p.enemy.stair_index)
        p.release = detail::stair_pair(s, *p.enemy.stair_index);
      p.flags = (p.flags & ~0xc00000u) | 1u;
      p.secondary &= ~8u;
      p.modes = {1, 1, 1, 0};
    }
    break;
  case 5:
    if (p.motion.flags & 0x2000000) {
      p.secondary_flags &= ~1u;
      p.modes = {1, 1, 1, 0};
    }
    break;
  }
  p.commit();
}
} // namespace ashford
