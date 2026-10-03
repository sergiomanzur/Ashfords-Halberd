// Native startup parameters from MIT recvx-decomp en01.c.
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
namespace ashford {
namespace {
constexpr ZombiePersonality personalities[] = {
    {30, 70, 14563, 0, 0},   {30, 50, 10922, 0, 5},   {30, 70, 14563, 10, 10},
    {50, 100, 14563, 10, 5}, {50, 90, 10922, 10, 8},  {40, 80, 10922, 40, 10},
    {50, 100, 14563, 10, 5}, {50, 90, 10922, 10, 8},  {40, 80, 10922, 40, 10},
    {40, 80, 10922, 20, 10}, {30, 70, 14563, 0, 0},   {30, 50, 10922, 0, 0},
    {30, 70, 14563, 30, 5},  {30, 50, 10922, 30, 5},  {30, 70, 14563, 0, 5},
    {40, 90, 14563, 20, 10}, {40, 80, 10922, 20, 10}, {40, 90, 14563, 40, 10},
    {40, 80, 10922, 50, 10}, {20, 50, 14563, -20, 0}, {20, 50, 10922, -30, 0},
    {30, 70, 14563, 0, 0},   {20, 50, 10922, 30, 0},  {30, 70, 14563, 50, 10},
    {30, 70, 14563, 0, 10},  {30, 70, 14563, 20, 10}, {20, 50, 10922, 30, 0}};
bool valid_choice(int32_t choice) {
  return choice >= 0 && choice <= 4 && !(choice & 1);
}
} // namespace
uint8_t zombie_personality_index(uint8_t model_version) {
  static constexpr uint8_t lookup[][2] = {
      {0, 0},   {1, 1},   {31, 2},  {18, 0},  {2, 3},   {3, 4},   {4, 5},
      {32, 6},  {33, 7},  {34, 8},  {21, 9},  {19, 3},  {5, 10},  {6, 11},
      {35, 12}, {36, 13}, {22, 14}, {20, 10}, {7, 15},  {8, 16},  {37, 17},
      {38, 18}, {24, 15}, {9, 19},  {10, 20}, {25, 21}, {39, 22}, {40, 23},
      {41, 24}, {42, 25}, {43, 26}};
  // bhEne01_Init iterates only the first 30 of its 31 table rows.
  for (size_t i = 0; i < 30; ++i)
    if (lookup[i][0] == model_version)
      return lookup[i][1];
  return 0;
}
const ZombiePersonality &zombie_personality(uint8_t model_version) {
  return zombie_personality_at(zombie_personality_index(model_version));
}
const ZombiePersonality &zombie_personality_at(uint8_t index) {
  if (index >= sizeof(personalities) / sizeof(*personalities))
    throw std::runtime_error("Zombie personality outside original table");
  return personalities[index];
}
int32_t roll_zombie_health(ScenarioState &state, uint8_t model_version) {
  static constexpr int32_t health[2][16] = {
      {60, 60, 60, 60, 60, 90, 90, 90, 90, 90, 120, 120, 120, 120, 120, 120},
      {20, 20, 40, 40, 40, 40, 40, 40, 40, 40, 60, 60, 60, 60, 60, 60}};
  auto base = health[state.game_mode == 2 ? 1 : 0][scenario_random(state) % 16];
  return base + (base * zombie_personality(model_version).health_percent) / 100;
}
int32_t select_zombie_walk(ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size() ||
      state.enemies[work].id != 1 || !(state.enemies[work].flags & 1) ||
      (state.enemies[work].flags & 0x80))
    throw std::runtime_error("Invalid unlinked zombie walking context");
  const ScenarioEntity *previous = nullptr;
  for (const auto &enemy : state.enemies)
    if (enemy.id == 1 && (enemy.flags & 1) && !(enemy.flags & 0x80)) {
      if (&enemy == &state.enemies[work])
        break;
      previous = &enemy;
    }
  int32_t choice;
  if (previous) {
    if (!previous->zombie_walk_choice ||
        !valid_choice(*previous->zombie_walk_choice))
      throw std::runtime_error("Earlier zombie walking work is uninitialized");
    choice = (*previous->zombie_walk_choice + 2) % 6;
  } else
    choice = int32_t(scenario_random(state) % 3) * 2;
  state.enemies[work].zombie_walk_choice = choice;
  return choice;
}
uint32_t zombie_walk_motion(int32_t choice, bool linked_body) {
  if (!valid_choice(choice))
    throw std::runtime_error("Zombie walking choice outside original pairs");
  static constexpr uint32_t motions[] = {0, 200, 40, 240, 41, 241};
  return motions[choice + (linked_body ? 1 : 0)];
}
void initialize_zombie_type(ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie type work pool");
  auto &zombie = state.enemies[work];
  if (zombie.id != 1 || !(zombie.flags & 1) || zombie.type >= 16)
    throw std::runtime_error("Zombie type outside original initializer table");
  const bool linked = zombie.flags & 0x80;
  const ScenarioEntity *parent = nullptr;
  if (linked) {
    if (!zombie.linked_parent ||
        *zombie.linked_parent >= state.enemies.size() ||
        *zombie.linked_parent == work ||
        !(state.enemies[*zombie.linked_parent].flags & 1))
      throw std::runtime_error(
          "Linked zombie type requires active parent work");
    parent = &state.enemies[*zombie.linked_parent];
  }
  const auto type = zombie.type;
  const bool half =
      type == 2 || type == 3 || type == 6 || type == 7 || type == 9;
  // Validate/read the preceding walking work before mutating other fields.
  if (!linked && type != 2 && type != 3 && type != 9)
    select_zombie_walk(state, work);
  zombie.modes = {1, 0, uint8_t(type == 6 || type == 7 || type == 15 ? 4 : 0),
                  0};
  if (half) {
    zombie.health = linked ? parent->health : zombie.health / 2;
    zombie.zombie_flags &= ~0xfu;
    if (!linked)
      zombie.zombie_flags |= 1;
    zombie.motion_number = linked ? 203 : 3;
  } else
    zombie.motion_number = linked ? 202 : 2;
  if (type == 10) {
    zombie.flags &= ~0x20u;
    zombie.zombie_flags &= ~0x10000000u;
  }
  if (type == 11 || type == 15)
    zombie.zombie_secondary_flags |= 0x10;
  if (type == 15) {
    zombie.motion_number = linked ? 213 : 13;
    zombie.motion_step = 0;
    if (!linked)
      zombie.zombie_flags = (zombie.zombie_flags & ~0xfu) | 0x40001;
  }
}
namespace {
size_t pose_owner(const ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie pose work pool");
  const auto owner = state.enemies[work].pose_owner.value_or(work);
  if (owner >= state.enemies.size() || !(state.enemies[owner].flags & 1) ||
      !state.enemies[owner].model || !state.enemies[work].motions ||
      (owner != work && state.enemies[owner].pose_owner))
    throw std::runtime_error("Invalid zombie model-work owner");
  return owner;
}
void prepare_pose(ScenarioEntity &owner) {
  if (owner.joint_pose.empty())
    owner.joint_pose = actor_bind_pose(*owner.model);
  if (!owner.model_work) {
    ActorPlacement placement;
    placement.flags = owner.flags;
    placement.position = owner.position;
    placement.angles = owner.angles;
    owner.model_work = calculate_actor_model_work(
        *owner.model, owner.joint_pose, {}, placement);
  }
  if (owner.model_work->joints.size() < 18)
    throw std::runtime_error("Zombie pose lacks original body joints");
}
Vec3 joint_origin(const ModelMatrix &m) {
  return {float(m[3]), float(m[7]), float(m[11])};
}
Vec3 joint_point(const ModelMatrix &m, Vec3 p) {
  Vec3 out{float(m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3]),
           float(m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7]),
           float(m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11])};
  if (!std::isfinite(out.x) || !std::isfinite(out.y) || !std::isfinite(out.z))
    throw std::runtime_error("Zombie capsule transform overflow");
  return out;
}
void calculate_startup_capsule(ScenarioState &state, size_t work) {
  auto &zombie = state.enemies[work];
  const auto owner_index = pose_owner(state, work);
  auto &owner = state.enemies[owner_index];
  if (zombie.flags & 0x80) {
    if (!zombie.linked_parent || *zombie.linked_parent != owner_index ||
        !owner.zombie_work)
      throw std::runtime_error("Attached zombie capsule lacks parent work");
    ActorPlacement placement;
    placement.flags = owner.flags;
    placement.position = owner.position;
    placement.angles = owner.angles;
    owner.model_work = calculate_actor_model_work(
        *owner.model, owner.joint_pose, *owner.model_work, placement);
    owner.rendered_model = pose_actor_model_work(*owner.model, owner.joint_pose,
                                                 *owner.model_work);
    const auto &matrices = owner.model_work->joints;
    if (owner.zombie_work->left_leg_state == -1 ||
        owner.zombie_work->right_leg_state == -1) {
      owner.geometry.capsule_start =
          joint_point(matrices.at(9).matrix, {0, 3.5f, 0});
      owner.geometry.capsule_end =
          joint_point(matrices.at(9).matrix, {0, -6, 0});
    } else {
      owner.geometry.capsule_start = joint_origin(matrices.at(10).matrix);
      const auto a = joint_origin(matrices.at(4).matrix),
                 b = joint_origin(matrices.at(7).matrix);
      owner.geometry.capsule_end = {(a.x + b.x) / 2, (a.y + b.y) / 2,
                                    (a.z + b.z) / 2};
    }
    owner.geometry.capsule_radius = 2;
  } else if (zombie.zombie_flags & 0x80000000u) {
    throw std::runtime_error(
        "Detached zombie requires suffix model calculation");
  }
  if (zombie.zombie_secondary_flags & 0x200) {
    zombie.geometry.collision_radius += .2f;
    if (zombie.geometry.collision_radius >= 2.5f) {
      zombie.geometry.collision_radius = 2.5f;
      zombie.zombie_secondary_flags &= ~0x200u;
    }
  }
  zombie.geometry.height = zombie.geometry.collision_height =
      float(owner.model_work->joints.at(10).matrix[7]) - zombie.position.y;
}
void apply_zombie_motion(ScenarioState &state, size_t work) {
  const auto owner_index = pose_owner(state, work);
  auto &zombie = state.enemies[work];
  if (zombie.id != 1 || !(zombie.flags & 1) || !zombie.zombie_work ||
      zombie.blend_count < 0 || zombie.motion_number >= zombie.motions->size())
    throw std::runtime_error("Invalid initialized zombie motion work");
  auto &owner = state.enemies[owner_index];
  prepare_pose(owner);
  zombie.motion_boundary = 0;
  const auto &slot = (*zombie.motions)[zombie.motion_number];
  if (!slot)
    return; // bhSetMotion leaves counters untouched for a null record.
  MotionApplicationOptions options;
  options.mode = zombie.motion_mode;
  const bool body = zombie.zombie_flags & 0x80000000u;
  options.joint_base = body ? 8 : 0;
  options.mirror_map =
      body ? std::vector<int8_t>{0, 1, 2, 3, 7, 8, 9, 4, 5, 6, -1}
           : std::vector<int8_t>{0,  1,  5,  6,  7,  2,  3,  4,  8, 9,
                                 10, 11, 15, 16, 17, 12, 13, 14, -1};
  if (zombie.blend_count) {
    const auto remaining_bits = 65536u - uint32_t(zombie.blend_rate);
    const auto remaining =
        remaining_bits < 0x80000000u
            ? int32_t(remaining_bits)
            : int32_t(int64_t(remaining_bits) - 0x100000000ll);
    options.blend_rate =
        zombie.motion_mode & 0x20
            ? 1.f / zombie.blend_count - .000015258789f * zombie.blend_rate
            : .000015258789f * remaining;
  }
  auto applied = apply_motion_pose(owner.joint_pose, *slot, zombie.frame,
                                   options, state.motion_angle_scratch);
  auto frame = zombie.frame;
  auto direction = zombie.frame_direction;
  const auto boundary =
      advance_motion_frame(slot->frames, int32_t(zombie.motion_step),
                           zombie.motion_mode, frame, direction);
  uint16_t attribute = zombie.motion_attribute;
  if (!slot->attributes.empty())
    attribute = slot->attributes.at(size_t(zombie.frame) >> 16);
  owner.joint_pose = std::move(applied.pose);
  state.motion_angle_scratch = applied.angle_scratch;
  zombie.frame = frame;
  zombie.frame_direction = direction;
  zombie.motion_boundary = boundary;
  zombie.motion_attribute = attribute;
  if (zombie.blend_count)
    --zombie.blend_count;
}
void initialize_work(ScenarioState &state, size_t work) {
  if (!state.collision || state.enemies.size() != 128 ||
      work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie initialization context");
  auto &zombie = state.enemies[work];
  if (zombie.id != 1 || !(zombie.flags & 1) || zombie.type >= 16 ||
      zombie.flags & 0x1000)
    throw std::runtime_error("Unsupported zombie initialization work");
  if (zombie.zombie_work && (zombie.zombie_flags & 0x80000000u))
    zombie.flags |= 0x80;
  const bool linked = zombie.flags & 0x80;
  const auto owner_index = pose_owner(state, work);
  if ((!linked && zombie.model_version != 9 && zombie.model_version != 10 &&
       zombie.model_version != 32) ||
      (linked &&
       (!zombie.linked_parent || *zombie.linked_parent != owner_index ||
        !state.enemies[owner_index].zombie_work)))
    throw std::runtime_error(
        "Zombie startup model or parent requires its controller path: slot " +
        std::to_string(work) + ", model version " +
        std::to_string(zombie.model_version));
  auto &owner = state.enemies[owner_index];
  prepare_pose(owner); // Finish-room bind matrices precede the startup sample.
  state.player_work_flags &= ~4u;
  zombie.geometry.radius = 3.8f;
  zombie.geometry.height = 18.2f;
  zombie.geometry.width = zombie.geometry.depth = 0;
  zombie.geometry.collision_radius = 2.5f;
  zombie.geometry.collision_height = 18.2f;
  zombie.state_flags = 0;
  zombie.damage.fill(0);
  zombie.blend_rate = zombie.blend_count = 0;
  zombie.frame = 0;
  zombie.motion_step = 65536;
  zombie.motion_number = 0;
  zombie.motion_mode = 0;
  if (!zombie.zombie_work) {
    zombie.zombie_work.emplace();
    if (!linked) {
      auto allocate = [&](int joint, uint16_t id) {
        auto slot = link_zombie_enemy(state, work, joint, id);
        if (!slot)
          throw std::runtime_error("Zombie linked work pool exhausted");
        return *slot;
      };
      zombie.zombie_work->body = allocate(1, 1);
      zombie.zombie_work->children[4] = allocate(0, 31);
      state.enemy_callbacks[31] = EnemyCallbackKind::ZombieHead;
      zombie.zombie_work->children[0] = allocate(0, 32);
      state.enemy_callbacks[32] = EnemyCallbackKind::ZombieArm;
      zombie.zombie_work->children[1] = allocate(0, 32);
      zombie.zombie_work->children[2] = allocate(0, 33);
      state.enemy_callbacks[33] = EnemyCallbackKind::ZombieLeg;
      zombie.zombie_work->children[3] = allocate(0, 33);
      bool first = true;
      for (size_t i = 0; i < state.enemies.size(); ++i) {
        const auto &enemy = state.enemies[i];
        if (enemy.id == 1 && enemy.flags & 1 && enemy.type != 10 &&
            !(enemy.flags & 0x80)) {
          first = i == work;
          break;
        }
      }
      if (first) {
        auto &manager = state.enemies[allocate(0, 38)];
        manager.linked_parent.reset();
        manager.flags &= ~0x80u;
        state.enemy_callbacks[38] = EnemyCallbackKind::ZombieManager;
      }
    }
  } else {
    const auto previous = *zombie.zombie_work;
    zombie.zombie_work.emplace();
    for (size_t i = 0; i < 7; ++i)
      zombie.zombie_work->children[i] = previous.children[i];
    zombie.zombie_work->body = previous.body;
    zombie.zombie_work->shadow = previous.shadow;
  }
  zombie.zombie_flags = zombie.zombie_secondary_flags = 0;
  zombie.zombie_walk_choice = 0;
  zombie.zombie_work->personality =
      zombie_personality_index(zombie.model_version);
  zombie.health = roll_zombie_health(state, zombie.model_version);
  zombie.zombie_flags |= 1;
  zombie.flags = (zombie.flags | 0x178) & ~2u;
  if (linked) {
    zombie.flags = (zombie.flags & ~0x28u) | 0x8000;
    zombie.zombie_flags = (zombie.zombie_flags & 0xf) | 0x80000000u;
    zombie.state_flags |= 8;
    zombie.type = owner.type;
    zombie.health = owner.health;
    zombie.position = owner.position;
    zombie.angles = {};
  } else {
    zombie.model_flags = 0;
    zombie.zombie_flags |= 0x10000000;
    if (zombie.zombie_work->body) {
      auto &body = state.enemies.at(*zombie.zombie_work->body);
      body.type = zombie.type;
      body.modes = zombie.modes;
    }
    if (!(zombie.zombie_work->material_color & 0xffffff)) {
      zombie.model_flags &= ~0x400u;
      zombie.zombie_work->material_color = 0xffb2b2b2;
    }
  }
  if (!(zombie.flags & 0x800)) {
    zombie.zombie_work->shadow =
        EnemyShadow{linked ? std::vector<int8_t>{8, 10, -1}
                           : std::vector<int8_t>{0, 3, 4, 10, -1},
                    linked ? 10 : 1,
                    {6, 5, 5}};
    zombie.flags |= 0x800;
  }
  initialize_zombie_type(state, work);
  zombie.clip_joints = {11, 14, 17, 4, 7, 12, 15, 1};
  zombie.model_flags &= ~0x20u;
  zombie.look_joint = 10;
  zombie.combo_flags |= 2;
  apply_zombie_motion(state, work);
  calculate_startup_capsule(state, work);
}
} // namespace
void initialize_zombie_work(ScenarioState &state, size_t work) {
  auto replacement = state;
  initialize_work(replacement, work);
  state = std::move(replacement);
}
void advance_zombie_motion(ScenarioState &state, size_t work) {
  auto replacement = state;
  apply_zombie_motion(replacement, work);
  state = std::move(replacement);
}
void initialize_zombie_auxiliary(ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie auxiliary work pool");
  const auto &current = state.enemies[work];
  if (!(current.flags & 1) || (current.id != 31 && current.id != 32 &&
                               current.id != 33 && current.id != 38))
    throw std::runtime_error("Unsupported zombie auxiliary initialization");
  auto replacement = current;
  replacement.state_flags = 0;
  replacement.flags |= 0x8000;
  if (replacement.id != 38) {
    const size_t words = replacement.id == 31 ? 8 : 28;
    if (!replacement.zombie_part_work)
      replacement.zombie_part_work =
          ZombiePartExtendedWork{std::vector<uint32_t>(words)};
    if (replacement.zombie_part_work->words.size() != words)
      throw std::runtime_error("Zombie auxiliary extended work size mismatch");
    auto &fields = replacement.zombie_part_work->words;
    fields[0] = fields[1] = fields[5] = 0;
    fields[2] = replacement.id == 31 ? 0 : 2;
  }
  replacement.modes = {1, 0, 0, 0};
  state.enemies[work] = std::move(replacement);
}
void tick_zombie_auxiliary(ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie auxiliary work pool");
  const auto &current = state.enemies[work];
  if (!(current.flags & 1) ||
      (current.id != 31 && current.id != 32 && current.id != 33 &&
       current.id != 38) ||
      current.modes[0] > 1)
    throw std::runtime_error("Unsupported zombie auxiliary controller state");
  if (!current.modes[0]) {
    initialize_zombie_auxiliary(state, work);
    return;
  }
  if (current.id != 38)
    return; // Original Head/Arm/Leg_MV00 callbacks are empty.
  // en01sub.c Parent_Move. Stage only the scalar fields it changes, avoiding
  // copies of render geometry and unrelated actor state on every frame.
  std::array<uint32_t, 128> primary{}, secondary{};
  std::array<size_t, 10> outside_floor{};
  size_t outside_count = 0, on_floor = 0;
  std::optional<size_t> nearest;
  float distance = 0;
  const bool stair_rule = state.stage == 0 || state.room == 6;
  for (size_t i = 0; i < state.enemies.size(); ++i) {
    const auto &root = state.enemies[i];
    primary[i] = root.flags;
    secondary[i] = root.zombie_secondary_flags;
    if (root.id != 1 || !(root.flags & 1) || root.type == 10 ||
        !root.zombie_work || root.state_flags & 0x1000000 || root.flags & 0x82)
      continue;
    const auto value = root.zombie_work->player_distance;
    if (!std::isfinite(value))
      throw std::runtime_error("Nonfinite zombie cached player distance");
    secondary[i] &= ~2u;
    if (!nearest || distance > value) {
      nearest = i;
      distance = value;
    }
    if (stair_rule) {
      primary[i] &= ~0x200000u;
      if (root.floor == 2)
        ++on_floor;
      else {
        if (outside_count == outside_floor.size())
          throw std::runtime_error(
              "Zombie stair policy exceeds original scratch capacity");
        outside_floor[outside_count++] = i;
      }
    }
  }
  if (nearest)
    secondary[*nearest] |= 2;
  if (stair_rule && on_floor >= 2)
    for (size_t i = 0; i < outside_count; ++i)
      primary[outside_floor[i]] |= 0x200000;
  for (size_t i = 0; i < state.enemies.size(); ++i) {
    state.enemies[i].flags = primary[i];
    state.enemies[i].zombie_secondary_flags = secondary[i];
  }
}
} // namespace ashford
