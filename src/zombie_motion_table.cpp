#include "ninja_matrix.hpp"
#include "zombie.hpp"
#include "zombie_motion_table_internal.hpp"
#include "zombie_motion_tables.hpp"
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::CheckMtnTbl and
// zonzon.c::bhEne_SetVibration. Queues retain source requests for backends.
namespace ashford {
namespace detail {
namespace {
size_t owner(const ScenarioState &state, size_t actor) {
  if (actor >= state.enemies.size())
    throw std::runtime_error("Invalid motion fix actor");
  const auto index = state.enemies[actor].pose_owner.value_or(actor);
  if (index >= state.enemies.size() ||
      (index != actor && state.enemies[index].pose_owner))
    throw std::runtime_error("Invalid motion fix pose owner");
  return index;
}
size_t parent(const ScenarioState &state, size_t actor) {
  const auto index = state.enemies[actor].linked_parent;
  if (!index || *index >= state.enemies.size() || *index == actor)
    throw std::runtime_error("Invalid motion fix parent");
  return *index;
}
} // namespace
MotionFixResult evaluate_zombie_motion_fix(const ScenarioState &state,
                                           size_t work, int32_t type,
                                           bool respect_mirror) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie motion fix pool");
  const auto &enemy = state.enemies[work];
  const bool mirror = respect_mirror && (enemy.motion_mode & 2);
  size_t target = work, tree = 0;
  Vec3 offset{};
  float factor = 1;
  switch (type) {
  case 0:
    tree = mirror ? 1 : 0;
    break;
  case 1:
    tree = mirror ? 0 : 1;
    break;
  case 2:
    tree = 2;
    break;
  case 3:
    tree = 3;
    break;
  case 6:
    tree = 6;
    break;
  case 7:
  case 8:
    tree = type == 7 ? (mirror ? 1 : 0) : (mirror ? 0 : 1);
    offset = {0, -1, -1.9f};
    factor = enemy.motion_number == 125 ? 1.2f : 1;
    break;
  case 10:
  case 11:
  case 12:
  case 13: {
    if (!enemy.zombie_work)
      throw std::runtime_error("Missing motion fix leg state");
    const bool left = type == 10 || type == 12;
    if ((left ? enemy.zombie_work->left_leg_state
              : enemy.zombie_work->right_leg_state) < 0)
      return {true, std::nullopt};
    if (type >= 12) {
      target = parent(state, work);
      tree = left ? 2 : 3;
    } else
      tree = left ? 4 : 5;
    break;
  }
  default:
    return {};
  }
  const auto &actor = state.enemies[target];
  const auto &posed = state.enemies[owner(state, target)];
  if (!posed.model_work)
    throw std::runtime_error("Missing cached motion fix model work");
  ActorPlacement placement;
  placement.flags = actor.flags;
  placement.position = actor.position;
  placement.angles = actor.angles;
  if (actor.flags & 0x80) {
    const auto &linked = state.enemies[owner(state, parent(state, target))];
    if (!linked.model_work || actor.linked_joint < 0 ||
        size_t(actor.linked_joint) >= linked.model_work->joints.size())
      throw std::runtime_error("Missing motion fix parent cached joint");
    placement.parent_joint =
        linked.model_work->joints[actor.linked_joint].matrix;
    placement.linked_offset = actor.linked_offset;
  }
  const auto delta =
      calculate_actor_fix_offset(posed.joint_pose, *posed.model_work, placement,
                                 zombie_fix_paths[tree], offset);
  auto position = actor.position;
  position.x = ninja_finite(position.x - factor * delta.x);
  position.z = ninja_finite(position.z - factor * delta.z);
  return {false, MotionCorrection{target, position}};
}
} // namespace detail
void tick_zombie_motion_table(ScenarioState &state, size_t work, int32_t frame,
                              const std::vector<int32_t> &playing) {
  if (state.enemies.size() != 128 || work >= state.enemies.size() ||
      !(state.enemies[work].flags & 1) || state.enemies[work].id != 1)
    throw std::runtime_error("Invalid zombie motion table callback work");
  const auto &enemy = state.enemies[work];
  if (state.room_motions.get() == enemy.motions.get())
    return;
  std::optional<detail::MotionCorrection> correction;
  if (enemy.flags & 0x40000) {
    if (enemy.motion_number == 2) {
      correction = detail::evaluate_zombie_motion_fix(
                       state, work, enemy.flags & 0x80000 ? 0 : 1, false)
                       .correction;
    } else {
      for (const auto &row : detail::zombie_motion_fixes) {
        if (row.motion == -1)
          break;
        if (uint32_t(row.motion) != enemy.motion_number)
          continue;
        for (const auto &window : row.windows) {
          if (window.type == -1 || frame < window.first || frame > window.last)
            continue;
          const auto fix =
              detail::evaluate_zombie_motion_fix(state, work, window.type);
          if (fix.skip)
            continue;
          correction = fix.correction;
          break;
        }
      }
    }
  }
  const auto origin = correction && correction->actor == work
                          ? correction->position
                          : enemy.position;
  auto seed = state.random_seed;
  std::array<EnemySoundRequest, 4> sounds{};
  size_t sound_count = 0;
  for (const auto &row : detail::zombie_motion_sounds) {
    if (row.motion == -1)
      break;
    if (uint32_t(row.motion) != enemy.motion_number)
      continue;
    for (const auto &event : row.events) {
      if (event.frame == -1)
        break;
      if (event.frame == frame) {
        if (auto code = choose_zombie_sound(enemy, seed, event.code, playing)) {
          detail::ninja_finite(origin.x);
          detail::ninja_finite(origin.y);
          detail::ninja_finite(origin.z);
          sounds.at(sound_count++) = {work, origin, *code};
        }
      }
    }
  }
  std::array<EnemyVibrationRequest, 4> vibrations{};
  size_t vibration_count = 0;
  if (enemy.modes[0] < 5) {
    for (const auto &row : detail::zombie_motion_vibrations) {
      if (row.motion == -1)
        break;
      if (uint32_t(row.motion) != enemy.motion_number)
        continue;
      for (const auto &event : row.events) {
        if (event.frame == -1)
          break;
        if (event.frame == frame && event.code >= 0 && event.code <= 2)
          vibrations.at(vibration_count++) = {work, 1, uint8_t(9 + event.code)};
      }
    }
  }
  if (sound_count)
    state.pending_enemy_sounds.reserve(state.pending_enemy_sounds.size() +
                                       sound_count);
  if (vibration_count)
    state.pending_enemy_vibrations.reserve(
        state.pending_enemy_vibrations.size() + vibration_count);
  if (correction)
    state.enemies[correction->actor].position = correction->position;
  state.random_seed = seed;
  for (size_t i = 0; i < sound_count; ++i)
    state.pending_enemy_sounds.push_back(sounds[i]);
  for (size_t i = 0; i < vibration_count; ++i)
    state.pending_enemy_vibrations.push_back(vibrations[i]);
}
} // namespace ashford
