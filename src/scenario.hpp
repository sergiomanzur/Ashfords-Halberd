#pragma once
#include "actor.hpp"
#include "assets.hpp"
#include "collision.hpp"
#include <array>
#include <memory>
#include <optional>
namespace ashford {
class RoomScenario;
class ScenarioProgram {
public:
  size_t size() const { return block_->size() - offset_; }
  uint8_t operator[](size_t index) const;
  size_t origin() const { return offset_; }

private:
  ScenarioProgram() = default;
  std::shared_ptr<const Bytes> block_;
  size_t offset_ = 0;
  friend class RoomScenario;
  friend RoomScenario decode_room_scenario(const Bytes &data);
};
// EVT_WORK disk offsets are relative to the event block, not process pointers.
// Slots 0/1 contain room initialization/control; event IDs start at slot 2.
class RoomScenario {
public:
  size_t entry_count() const { return programs_.size(); }
  const ScenarioProgram &program(size_t slot) const;
  const ScenarioProgram &event(size_t id) const;

private:
  std::vector<ScenarioProgram> programs_;
  friend RoomScenario decode_room_scenario(const Bytes &data);
};
RoomScenario decode_room_scenario(const Bytes &data);
struct ScenarioDoor {
  uint16_t attribute = 0;
  uint8_t stage = 0, room = 0, position = 0, type = 0;
};
struct ScenarioEntity {
  uint32_t flags = 0, state_flags = 0;
  int32_t angular_speed = 0;
  int32_t health = 0;
  Vec3 position{}, previous_position{};
  std::array<int32_t, 3> angles{}, previous_angles{};
  int8_t floor = 0;
  uint32_t motion_mode = 0, model_flags = 0;
  float blend_rate = 0;
  std::shared_ptr<const ActorModel> model;
  std::vector<JointPose> joint_pose;
  std::optional<Scene> rendered_model;
  uint16_t id = 0;
  std::array<uint8_t, 4> modes{};
  uint32_t motion_number = 0, motion_step = 65536;
  int32_t frame = 0, blend_count = 0, control1 = 0, control2 = 0;
  std::shared_ptr<const std::vector<std::optional<Motion>>> motions;
};
struct ScenarioTask {
  std::optional<ScenarioProgram> program;
  bool active = false;
  size_t pc = 0;
  int loop = -1;
  std::array<int32_t, 4> counts{};
  std::array<size_t, 6> loop_bodies{}, loop_conditions{};
  int32_t count2 = 0, count3 = 0;
  // Stable indices, rather than pointers into caller-owned entity vectors.
  uint8_t work_group = 0, model_index = 0;
  std::optional<size_t> work_index;
  std::array<uint8_t, 3> position_signs{}, angle_signs{};
  uint32_t motion_mode = 0, model_flags = 0;
  float blend_rate = 0;
};
struct ScenarioState {
  std::array<std::array<uint32_t, 32>, 17> flags{};
  uint8_t room_case = 0, interaction_index = 0, floor_index = 0;
  uint8_t stage = 0, room = 0, player_sleep_ticks = 0;
  int32_t room_sound_case = 0;
  std::optional<ScenarioDoor> door;
  // Caller binds current runtime entities, including the authored-enemy to
  // allocated-work-slot map produced during room loading.
  std::vector<ScenarioEntity> enemies, objects, items;
  std::vector<size_t> enemy_slots;
  std::vector<CollisionRecord> interactions;
  std::optional<RoomCollision> collision;
  std::shared_ptr<const std::vector<std::optional<Motion>>> room_motions;
  std::shared_ptr<const RoomScenario> scenario;
  std::array<ScenarioTask, 16> tasks;
  uint32_t event_timer = 0;
  int conditional_depth = 0;
  int64_t branch_cursor = 0;
  std::array<uint32_t, 384> inventory{};
  uint8_t player_id = 0;
  int32_t equipped_weapon = 0, player_health = 0, message_selection = 0,
          status_timer = 0;
  int32_t player_frame = 0;
};
// Native control and event scheduling subset. Unsupported instructions fail
// explicitly; entity controllers and audiovisual instructions are pending.
void run_scenario(const Bytes &program, ScenarioState &state);
void run_scenario(const ScenarioProgram &program, ScenarioState &state);
size_t start_scenario_event(ScenarioState &state, size_t level, size_t event);
void tick_scenario_tasks(ScenarioState &state);
} // namespace ashford
