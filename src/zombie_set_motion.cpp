#include "enemy_motion_internal.hpp"
#include "enemy_movement.hpp"
#include "player.hpp"
#include "zombie.hpp"
#include "zombie_motion_internal.hpp"
#include "zombie_set_motion_tables.hpp"
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::bhEne01_SetMtn.
namespace ashford {
namespace {
struct ScalarSnapshot {
  size_t slot;
  detail::MotionState motion;
  uint32_t direction, zombie_flags, model_flags;
  uint16_t attribute;
  int boundary;
  Vec3 position;
  std::array<int32_t, 3> angles;
  float speed, shape;
  uint8_t reaction;
  ScalarSnapshot(const ScenarioState &state, size_t index) : slot(index) {
    const auto &e = state.enemies[index];
    motion = detail::snapshot(e);
    direction = e.frame_direction;
    zombie_flags = e.zombie_flags;
    model_flags = e.model_flags;
    attribute = e.motion_attribute;
    boundary = e.motion_boundary;
    position = e.position;
    angles = e.angles;
    speed = e.speed;
    shape = e.shape_count;
    reaction = e.zombie_work->damage_rotation_frame;
  }
  void restore(ScenarioState &state) const noexcept {
    auto &e = state.enemies[slot];
    detail::commit(e, motion);
    e.frame_direction = direction;
    e.zombie_flags = zombie_flags;
    e.model_flags = model_flags;
    e.motion_attribute = attribute;
    e.motion_boundary = boundary;
    e.position = position;
    e.angles = angles;
    e.speed = speed;
    e.shape_count = shape;
    e.zombie_work->damage_rotation_frame = reaction;
  }
};
struct MotionTransaction {
  ScenarioState &state;
  size_t owner;
  ScalarSnapshot work, parent;
  bool has_parent;
  std::vector<JointPose> pose;
  std::array<uint16_t, 3> scratch;
  uint32_t seed;
  size_t sounds, vibrations;
  bool committed = false;
  MotionTransaction(ScenarioState &s, size_t index, size_t posed,
                    std::optional<size_t> linked)
      : state(s), owner(posed), work(s, index),
        parent(s, linked.value_or(index)),
        has_parent(linked && *linked != index),
        pose(s.enemies[posed].joint_pose), scratch(s.motion_angle_scratch),
        seed(s.random_seed), sounds(s.pending_enemy_sounds.size()),
        vibrations(s.pending_enemy_vibrations.size()) {}
  ~MotionTransaction() {
    if (committed)
      return;
    work.restore(state);
    if (has_parent)
      parent.restore(state);
    state.enemies[owner].joint_pose = std::move(pose);
    state.motion_angle_scratch = scratch;
    state.random_seed = seed;
    state.pending_enemy_sounds.resize(sounds);
    state.pending_enemy_vibrations.resize(vibrations);
  }
};
void zero_root(std::vector<JointPose> &pose, size_t joint) {
  if (joint >= pose.size() || !pose[joint].position)
    throw std::runtime_error("Missing sampled SetMtn root position");
  pose[joint].position->x = pose[joint].position->z = 0;
}
void move_by_speed(ScenarioEntity &enemy, int32_t frame, bool turn) {
  measure_enemy_motion_speed(enemy, frame, 0);
  if (turn && enemy.speed > .05f) {
    Vec3 target{enemy.zombie_work->target_x, 0, enemy.zombie_work->target_z};
    enemy.angles[1] = turn_enemy_yaw(enemy, target, 256);
  }
  enemy.position =
      player_add_speed(enemy.position, uint16_t(enemy.angles[1]), enemy.speed);
}
} // namespace
int tick_zombie_motion(ScenarioState &state, size_t work,
                       const ZombieMotionEffects &effects,
                       const std::vector<int32_t> &playing) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid SetMtn work pool");
  auto &enemy = state.enemies[work];
  if (!(enemy.flags & 1) || enemy.id != 1 || !enemy.zombie_work)
    throw std::runtime_error("Missing SetMtn zombie work");
  if (enemy.zombie_flags & 0x10)
    return 0;
  const auto owner = enemy.pose_owner.value_or(work);
  if (owner >= state.enemies.size() || !(state.enemies[owner].flags & 1) ||
      (owner != work && state.enemies[owner].pose_owner) ||
      !state.enemies[owner].model || !state.enemies[owner].model_work ||
      state.enemies[owner].joint_pose.size() < 18 ||
      state.enemies[owner].joint_pose.size() !=
          state.enemies[owner].model_work->joints.size())
    throw std::runtime_error("SetMtn requires initialized owner pose/cache");
  if (enemy.modes[0] < 5 && (!effects.kami || !effects.water))
    throw std::runtime_error(
        "SetMtn requires synchronous Kami/Water effect handlers");
  const bool body = enemy.zombie_flags & 0x80000000u;
  std::optional<size_t> parent;
  if (enemy.linked_parent && *enemy.linked_parent < state.enemies.size() &&
      (state.enemies[*enemy.linked_parent].flags & 1) &&
      state.enemies[*enemy.linked_parent].zombie_work)
    parent = enemy.linked_parent;
  if (body && (!parent || *parent == work))
    throw std::runtime_error("Missing SetMtn linked-body parent");
  MotionTransaction transaction(state, work, owner, parent);
  const auto frame = enemy.frame / 65536;
  detail::apply_zombie_motion_channels(state, work);
  auto &pose = state.enemies[owner].joint_pose;
  if (body) {
    if (enemy.zombie_flags & 0x2000000) {
      zero_root(pose, 8);
      move_by_speed(enemy, frame, true);
    }
    auto &root = state.enemies[*parent];
    if (root.zombie_flags & 0x8000000) {
      const auto index = enemy.zombie_work->damage_rotation_frame;
      if (index >= 8)
        throw std::runtime_error("SetMtn damage rotation outside source table");
      auto angles = pose[8].angles.value_or(std::array<int32_t, 3>{});
      const bool front = root.zombie_flags & 0x2000,
                 side = root.zombie_secondary_flags & 0x20;
      angles[0] = detail::zombie_damage_rotation[front ? 0 : 1][index];
      angles[1] = detail::zombie_damage_rotation[front == side ? 2 : 3][index];
      pose[8].angles = angles;
      if (++enemy.zombie_work->damage_rotation_frame >= 8) {
        root.zombie_flags &= ~0x8000000u;
        enemy.zombie_work->damage_rotation_frame = 0;
      }
    }
  } else if (enemy.zombie_flags & 0x1000000) {
    zero_root(pose, 0);
    translate_enemy_motion(enemy, frame, 0);
  } else if (enemy.zombie_flags & 0x2000000) {
    zero_root(pose, 0);
    move_by_speed(enemy, frame,
                  enemy.motion_number == 1 || enemy.motion_number == 32);
  }
  if (enemy.motion_boundary)
    enemy.flags |= 0x2000000;
  else
    enemy.flags &= ~0x2000000u;
  if (enemy.modes[0] < 5) {
    effects.kami(state, work, frame);
    effects.water(state, work, frame);
  }
  if (enemy.type == 10)
    enemy.zombie_flags &= ~0x10000000u;
  if (enemy.zombie_flags & 0x10000000) {
    auto sample = [&](const auto &table, int32_t index) {
      if (index < 0 || size_t(index) >= table.size())
        throw std::runtime_error("SetMtn mouth frame outside source table");
      enemy.model_flags |= 2;
      enemy.shape_count = table[index];
    };
    if (enemy.motion_number == 8 || enemy.motion_number == 120)
      sample(detail::zombie_bite_shape, frame);
    else if (enemy.motion_number == 85 || enemy.motion_number == 121)
      sample(detail::zombie_bite_shape2, frame);
    else if (enemy.motion_number == 0 || enemy.motion_number == 40 ||
             enemy.motion_number == 41 || enemy.motion_number == 125 ||
             enemy.motion_number == 117)
      sample(detail::zombie_chew_shape, frame % 40);
    else {
      enemy.model_flags &= ~2u;
      enemy.shape_count = 0;
    }
  }
  tick_zombie_motion_table(state, work, frame, playing);
  transaction.committed = true;
  return enemy.motion_boundary;
}
} // namespace ashford
