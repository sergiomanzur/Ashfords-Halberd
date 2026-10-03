#pragma once
#include "actor.hpp"
#include "assets.hpp"
#include "collision.hpp"
#include "face_controller.hpp"
#include <array>
#include <memory>
#include <optional>
namespace ashford {
class RoomScenario;
struct RoomEntity;
struct RoomRoutes;
struct NativeEffectPool;
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
struct EnemyGeometry {
  float radius = 0, height = 0, width = 0, depth = 0;
  float collision_radius = 0, collision_height = 0;
  Vec3 capsule_start{}, capsule_end{};
  float capsule_radius = 0;
};
struct EnemyShadow {
  std::vector<int8_t> joints;
  int32_t anchor_joint = 0;
  Vec3 size{};
};
struct ZombieExtendedWork {
  // EXP0 child slots: arms, legs, head, cap, worm, bomb and scope.
  std::array<std::optional<size_t>, 9> children{};
  std::optional<size_t> body;
  std::optional<EnemyShadow> shadow;
  uint32_t material_color = 0;
  uint8_t personality = 0;
  uint8_t awareness = 0, awareness_timeout = 0; // EXP0_UC(0x28/0x29).
  uint8_t route_id = 0, route_timer = 0;        // EXP0_UC(0x2a/0x2b).
  uint8_t damage_rotation_frame = 0;            // EXP0_UC(0x2c).
  float target_x = 0, target_z = 0;             // EXP0_F(0x58/0x60).
  bool path_blocked = false;                    // Nonzero EXP0 pointer(0x70).
  std::optional<size_t> stair_target;           // EXP0 pointer(0x74).
  int32_t neck_yaw = 0;                         // EXP0_I(0x50).
  // Separate source limb accumulators; CalcEnemy reads only88/8c.
  int32_t damage_80 = 0, damage_84 = 0, damage_88 = 0, damage_8c = 0;
  float player_distance =
      0; // EXP0_F(84), updated by the original Move callback.
};
struct ZombiePartExtendedWork {
  // en01sub.c stores 32 bytes for the head, 112 for an arm/leg. Retain
  // untouched words on reinitialization; damage-state meanings are separate.
  std::vector<uint32_t> words;
};
struct ScenarioEntity {
  uint32_t flags = 0, state_flags = 0, secondary_flags = 0;
  int32_t angular_speed = 0;
  int32_t health = 0;
  Vec3 position{}, previous_position{};
  Vec3 collision_offset{};
  std::array<int32_t, 3> angles{}, previous_angles{};
  int8_t floor = 0;
  uint32_t motion_mode = 0, model_flags = 0;
  int32_t blend_rate = 0;
  std::shared_ptr<const ActorModel> model;
  std::shared_ptr<const ActorModelBank> model_bank;
  std::shared_ptr<const ActorMorphModels> morph_models;
  std::vector<JointPose> joint_pose;
  std::optional<ActorModelWork> model_work;
  // BH_PWORK::mtx is per work, even when its ML_WORK cache is shared.
  std::optional<ModelMatrix> model_matrix;
  std::optional<FaceRuntime> face;
  // ML_WORK aliasing is represented by an owner slot, not mutable pointers.
  std::optional<size_t> pose_owner;
  std::optional<Scene> rendered_model;
  uint16_t id = 0, type = 0;
  uint8_t model_version = 0;
  uint8_t parameter = 0;
  int8_t draw_type = 0;
  std::array<int8_t, 4> hide{};
  std::array<uint8_t, 4> modes{};
  uint32_t motion_number = 0, motion_step = 65536;
  int32_t frame = 0, blend_count = 0, control1 = 0, control2 = 0;
  int32_t control0 = 0; // Original ct0, distinct from ct1/ct2/ct3.
  int32_t control3 = 0, highlight_yaw = 0;
  int16_t torso_yaw = 0;  // BH_PWORK::wax.
  int16_t turn_speed = 0; // BH_PWORK::way.
  float speed = 0;        // BH_PWORK::spd.
  float shape_count = 0;  // BH_PWORK::shp_ct, consumed by draw-time morphology.
  uint32_t frame_direction = 0;
  uint16_t motion_attribute = 0;
  int motion_boundary = 0;
  std::shared_ptr<const std::vector<std::optional<Motion>>> motions;
  std::array<int8_t, 8> clip_joints{};
  std::optional<size_t> linked_parent;
  int32_t linked_joint = 0;
  Vec3 linked_offset{};
  std::optional<int32_t> zombie_walk_choice;
  uint32_t zombie_flags = 0, zombie_secondary_flags = 0;
  std::optional<ZombieExtendedWork> zombie_work;
  std::optional<ZombiePartExtendedWork> zombie_part_work;
  EnemyGeometry geometry;
  std::array<int32_t, 64> damage{};
  int32_t damage_joint = 0; // BH_PWORK::djnt_no.
  Vec3 damage_velocity{};   // BH_PWORK::dvx/dvy/dvz.
  uint32_t combo_flags = 0;
  int32_t combo_timeout = 0;
  int32_t look_joint = 0;
  std::optional<size_t> stair_index; // kdnp/kdnidx in current-room ATR work.
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
enum class EnemyCallbackKind {
  Original,
  ZombieHead,
  ZombieArm,
  ZombieLeg,
  ZombieManager
};
struct EnemySoundRequest {
  size_t work = 0;
  Vec3 position;
  int32_t code = 0;
};
struct EnemyVibrationRequest {
  size_t work = 0;
  uint8_t channel = 1, preset = 9;
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
  // bhEne_SetCallFunc replaces shared auxiliary callback slots during startup.
  std::array<EnemyCallbackKind, 100> enemy_callbacks{};
  std::vector<CollisionRecord> interactions;
  std::optional<RoomCollision> collision;
  std::vector<CollisionRecord> dynamic_miscellaneous; // sys->metcp.
  std::vector<CollisionRecord> dynamic_walls;         // sys->mwalp.
  std::vector<CollisionRecord> dynamic_floors;        // sys->mflrp.
  // Immutable native effect state: active events replace only this pool value.
  std::shared_ptr<const NativeEffectPool> effect_pool;
  std::vector<EnemySoundRequest> pending_enemy_sounds;
  std::vector<EnemyVibrationRequest> pending_enemy_vibrations;
  std::shared_ptr<const RoomRoutes> routes;
  std::shared_ptr<const std::vector<std::optional<Motion>>> room_motions;
  std::shared_ptr<const std::vector<Bytes>> face_masks;
  std::shared_ptr<const RoomScenario> scenario;
  std::array<ScenarioTask, 16> tasks;
  size_t current_task = 0;
  uint32_t event_timer = 0;
  uint32_t random_seed = 1;
  // Motion.c's shared mka_ang belongs to this native runtime, not a global.
  std::array<uint16_t, 3> motion_angle_scratch{};
  int32_t game_mode = 0;
  int conditional_depth = 0;
  int64_t branch_cursor = 0;
  std::array<uint32_t, 384> inventory{};
  uint8_t player_id = 0;
  int32_t equipped_weapon = 0, player_health = 0, message_selection = 0,
          status_timer = 0;
  int32_t player_frame = 0;
  uint16_t selected_item_id = 0;
  uint32_t player_state_flags = 0;
  uint32_t player_work_flags = 0;
  uint32_t player_secondary_flags = 0;
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
// USA ELF rand: wrapped 32-bit state shared by native game subsystems.
uint32_t scenario_random(ScenarioState &state);
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
