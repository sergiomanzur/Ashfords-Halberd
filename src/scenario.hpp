#pragma once
#include "actor.hpp"
#include "assets.hpp"
#include "collision.hpp"
#include <array>
#include <memory>
#include <optional>
namespace ashford {
class RoomScenario;
struct RoomEntity;
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
  int32_t blend_rate = 0;
  std::shared_ptr<const ActorModel> model;
  std::shared_ptr<const ActorModelBank> model_bank;
  std::vector<JointPose> joint_pose;
  std::optional<Scene> rendered_model;
  uint16_t id = 0, type = 0;
  uint8_t model_version = 0;
  uint8_t parameter = 0;
  int8_t draw_type = 0;
  std::array<int8_t, 4> hide{};
  std::array<uint8_t, 4> modes{};
  uint32_t motion_number = 0, motion_step = 65536;
  int32_t frame = 0, blend_count = 0, control1 = 0, control2 = 0;
  int32_t control3 = 0, highlight_yaw = 0;
  uint32_t frame_direction = 0;
  uint16_t motion_attribute = 0;
  int motion_boundary = 0;
  std::shared_ptr<const std::vector<std::optional<Motion>>> motions;
  std::array<int8_t, 8> clip_joints{};
  std::optional<size_t> linked_parent;
  int32_t linked_joint = 0;
  Vec3 linked_offset{};
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
  int32_t blend_rate = 0;
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
  size_t current_task = 0;
  uint32_t event_timer = 0;
  uint32_t random_seed = 1;
  int conditional_depth = 0;
  int64_t branch_cursor = 0;
  std::array<uint32_t, 384> inventory{};
  uint8_t player_id = 0;
  int32_t equipped_weapon = 0, player_health = 0, message_selection = 0,
          status_timer = 0;
  int32_t player_frame = 0;
  uint16_t selected_item_id = 0;
  uint32_t player_state_flags = 0;
  uint32_t drawing_flags = 0;
};
// Native control and event scheduling subset. Unsupported instructions fail
// explicitly; entity controllers and audiovisual instructions are pending.
void run_scenario(const Bytes &program, ScenarioState &state);
void run_scenario(const ScenarioProgram &program, ScenarioState &state);
size_t start_scenario_event(ScenarioState &state, size_t level, size_t event);
void tick_scenario_tasks(ScenarioState &state);
void tick_scenario_actors(ScenarioState &state);
// Native simple object controllers and pickup highlight timing. Linked,
// animated-special, collision/fade objects require their separate controllers.
void tick_scenario_props(ScenarioState &state);
// bhInitEvent's native state/script ordering. Sound commands 3/4 are pending
// the audio backend. All room resources must already be bound.
void initialize_scenario(ScenarioState &state);
// bhControlEvent: room control uses the current command context, then runs
// the event scheduler if the room script enables it.
void control_scenario(ScenarioState &state);
// bhSetRoom/bhSetEnemy/bhFinishRoom enemy bindings. Replaces only enemies and
// their authored work-number map after all assets validate successfully.
void bind_room_enemies(const Bytes &data, const RoomCollision &collision,
                       ScenarioState &state);
// eneset.c::bhSetEnemy first-free allocation; an exhausted original pool
// returns no slot. Newly allocated work has no model/motion bindings yet.
std::optional<size_t> allocate_enemy_work(std::vector<ScenarioEntity> &pool,
                                        const RoomEntity &authored,
                                        const RoomCollision &collision);
// en01.c::bhEne01_SetLinkEnemy: hidden linked work shares immutable parent
// assets and stores its parent as a stable slot index, rather than a pointer.
std::optional<size_t> link_zombie_enemy(ScenarioState &state, size_t parent,
                                      int32_t joint, uint16_t id);
// Preserve attachment slots 0–3; replace authored objects/items only after
// validating all room assets. Controllers run separately.
void bind_room_props(const Bytes &data, ScenarioState &state);
} // namespace ashford
