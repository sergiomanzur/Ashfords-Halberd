#include "scenario_motion.hpp"
#include "effects.hpp"
#include "enemy_motion_internal.hpp"
#include "ninja_matrix.hpp"
#include "player.hpp"
#include "zombie_motion_internal.hpp"
#include <algorithm>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp event.c, MdlPut.c and pwksub.c.
namespace ashford {
namespace {
struct Key {
  uint8_t group;
  size_t work;
  bool operator==(const Key &other) const {
    return group == other.group && work == other.work;
  }
};
ScenarioEntity &entity(ScenarioState &s, Key key) {
  auto &pool = key.group == 1   ? s.enemies
               : key.group == 2 ? s.objects
               : key.group == 3
                   ? s.items
                   : throw std::runtime_error("Invalid model work group");
  if (key.work >= pool.size())
    throw std::runtime_error("Scenario model work outside pool");
  return pool[key.work];
}
Key owner(ScenarioState &s, Key key) {
  const auto &work = entity(s, key);
  if (!work.pose_owner)
    return key;
  if (key.group != 1 || *work.pose_owner == key.work)
    throw std::runtime_error("Invalid scenario pose-owner alias");
  Key result{1, *work.pose_owner};
  const auto &posed = entity(s, result);
  if (!(posed.flags & 1) || posed.pose_owner)
    throw std::runtime_error("Invalid scenario pose owner");
  return result;
}
const ActorModel &model(const ScenarioEntity &e) {
  if (e.face)
    return e.face->model.actor;
  if (!e.model)
    throw std::runtime_error("Missing scenario model resource");
  return *e.model;
}
struct CacheUpdate {
  Key key;
  ActorModelWork cache;
  std::vector<JointPose> prepared_pose;
};
struct BaseUpdate {
  Key key;
  ModelMatrix matrix;
};
// Stage only model work. Later linked works see earlier shared-cache writes,
// including hidden active children, without publishing a partial calculation.
class ModelCalculation {
  ScenarioState &s;
  std::vector<CacheUpdate> caches;
  std::vector<BaseUpdate> bases;
  std::shared_ptr<NativeEffectPool> effects;
  const ActorModelWork *cache(Key key) {
    key = owner(s, key);
    for (const auto &update : caches)
      if (update.key == key)
        return &update.cache;
    const auto &e = entity(s, key);
    return e.model_work ? &*e.model_work : nullptr;
  }
  ModelMatrix joint(size_t parent, int32_t number) {
    const auto &p = entity(s, {1, parent});
    if (!(p.flags & 1))
      throw std::runtime_error("Inactive scenario linked parent");
    const auto *c = cache({1, parent});
    if (!c || number < 0 || size_t(number) >= c->joints.size())
      throw std::runtime_error("Scenario linked joint outside parent cache");
    return c->joints[size_t(number)].matrix;
  }
  void calculate(Key key) {
    const auto &e = entity(s, key);
    const auto posed_key = owner(s, key);
    const auto &posed = entity(s, posed_key);
    const auto *previous = cache(key);
    auto calculated = previous ? *previous : ActorModelWork{};
    const auto old_base = calculated.base_matrix;
    ActorPlacement placement;
    placement.flags = e.flags;
    placement.position = e.position;
    placement.angles = e.angles;
    if (e.flags & 0x80) {
      if (!e.linked_parent || (key.group == 1 && *e.linked_parent == key.work))
        throw std::runtime_error("Invalid scenario linked parent");
      placement.parent_joint = joint(*e.linked_parent, e.linked_joint);
      placement.linked_offset = e.linked_offset;
    }
    if (e.model_matrix)
      calculated.base_matrix = *e.model_matrix;
    else if (!(posed_key == key)) {
      if ((e.flags & 0x1000) && !(e.flags & 0x80))
        throw std::runtime_error("Missing retained scenario actor base");
      calculated.base_matrix = detail::ninja_store(detail::ninja_identity());
    }
    auto pose = posed.joint_pose.empty() ? actor_bind_pose(model(posed))
                                         : std::vector<JointPose>{};
    calculated = calculate_actor_model_work(
        model(posed), pose.empty() ? posed.joint_pose : pose,
        std::move(calculated), placement);
    bases.push_back({key, calculated.base_matrix});
    if (!(posed_key == key))
      calculated.base_matrix = old_base;
    for (auto &update : caches)
      if (update.key == posed_key) {
        update.cache = std::move(calculated);
        if (!pose.empty())
          update.prepared_pose = std::move(pose);
        return;
      }
    caches.push_back({posed_key, std::move(calculated), std::move(pose)});
  }

public:
  explicit ModelCalculation(ScenarioState &state) : s(state) {}
  void run(Key key, bool self, bool links) {
    entity(s, key);
    if (self)
      calculate(key);
    if (links) {
      // Native links currently name enemy-pool parents. Authored prop-parent
      // links are rejected by binding until their original representation
      // exists.
      if (key.group != 1)
        throw std::runtime_error("Unported scenario prop-parent model links");
      auto children = [&](uint8_t group, size_t count) {
        for (size_t i = 0; i < count; ++i) {
          const Key child{group, i};
          const auto &e = entity(s, child);
          if (!(child == key) && (e.flags & 0x81) == 0x81 &&
              e.linked_parent == key.work && cache(child))
            calculate(child);
        }
      };
      children(1, s.enemies.size());
      children(2, s.objects.size());
      children(3, s.items.size());
      if (s.effect_pool)
        for (size_t i = 0; i < s.effect_pool->works.size(); ++i) {
          const auto &e = s.effect_pool->works[i];
          if ((e.flags & 0x81) != 0x81 || e.linked_parent != key.work)
            continue;
          auto matrix = detail::ninja_load(joint(key.work, e.linked_joint));
          detail::ninja_translate(matrix, e.offset);
          detail::ninja_rotate(matrix, e.angles);
          if (!effects)
            effects = std::make_shared<NativeEffectPool>(*s.effect_pool);
          effects->works[i].model_matrix = detail::ninja_store(matrix);
        }
    }
    for (auto &update : caches) {
      auto &e = entity(s, update.key);
      e.model_work = std::move(update.cache);
      if (!update.prepared_pose.empty())
        e.joint_pose = std::move(update.prepared_pose);
    }
    for (const auto &update : bases)
      entity(s, update.key).model_matrix = update.matrix;
    if (effects)
      s.effect_pool = std::move(effects);
  }
};
int32_t signed_word(uint32_t bits) {
  return bits < 0x80000000u ? int32_t(bits)
                            : int32_t(int64_t(bits) - 0x100000000ll);
}
void sample(ScenarioState &s, Key key, int32_t add, uint32_t mode) {
  auto &e = entity(s, key);
  if (key.group == 1 && e.id == 1) {
    detail::apply_zombie_motion_channels(s, key.work, add, mode);
    return;
  }
  const auto posed_key = owner(s, key);
  auto &posed = entity(s, posed_key);
  if (!e.motions || e.motion_number >= e.motions->size() || e.blend_count < 0)
    throw std::runtime_error("Invalid scenario motion binding");
  const auto &slot = (*e.motions)[e.motion_number];
  if (!slot) {
    e.motion_boundary = 0;
    return;
  }
  MotionApplicationOptions options;
  options.mode = mode;
  if (posed.model_work)
    for (const auto &j : posed.model_work->joints)
      options.joint_flags.push_back(uint8_t(j.flags));
  if (e.blend_count)
    options.blend_rate =
        mode & 0x20
            ? 1.f / e.blend_count - .000015258789f * e.blend_rate
            : .000015258789f * signed_word(65536u - uint32_t(e.blend_rate));
  auto pose = posed.joint_pose.empty() ? actor_bind_pose(model(posed))
                                       : std::vector<JointPose>{};
  auto applied =
      apply_motion_pose(pose.empty() ? posed.joint_pose : pose, *slot, e.frame,
                        options, s.motion_angle_scratch);
  auto frame = e.frame;
  auto direction = e.frame_direction;
  const auto boundary =
      advance_motion_frame(slot->frames, add, mode, frame, direction);
  const auto attribute =
      motion_attribute_at(*slot, e.frame, e.motion_attribute);
  posed.joint_pose = std::move(applied.pose);
  s.motion_angle_scratch = applied.angle_scratch;
  e.frame = frame;
  e.frame_direction = direction;
  e.motion_boundary = boundary;
  e.motion_attribute = attribute;
  if (e.blend_count)
    --e.blend_count;
}
} // namespace
void calculate_scenario_models(ScenarioState &s, uint8_t group, size_t work,
                               bool self, bool links) {
  ModelCalculation(s).run({group, work}, self, links);
}
void select_scenario_motion(ScenarioState &state, uint8_t group, size_t work,
                            const ScenarioMotionSelection &selection) {
  static constexpr uint32_t steps[] = {
      0x8000, 0x10000, 0x10000, 0x8000, 0x5555, 0x4000, 0x3333, 0x2aaa, 0x8000,
      0x4000, 0x2aaa,  0x2000,  0x1999, 0x1555, 0x2492, 0x2000, 0x1c71, 0x1999,
      0x1745, 0x1555,  0x1249,  0x1000, 0xe38,  0xccc,  0xba2,  0xaaa};
  if (selection.selector != 1)
    throw std::runtime_error("Unported scenario motion selector");
  if (selection.speed >= std::size(steps))
    throw std::runtime_error("Unsupported scenario motion speed");
  if (!state.room_motions || state.room_motions->size() != 512)
    throw std::runtime_error("Scenario motion selection without room bank");
  // Entry is transactional; retain live scheduler/task references in the
  // caller.
  auto s = state;
  const Key key{group, work};
  auto &root = entity(s, key);
  ScenarioEntity *body = nullptr;
  if (root.id == 1) {
    if (group != 1 || !root.zombie_work || !root.zombie_work->body)
      throw std::runtime_error("Scenario motion selection missing zombie body");
    const auto index = *root.zombie_work->body;
    body = &entity(s, {1, index});
    if (index == work || !(body->flags & 1) || body->id != 1 ||
        !body->zombie_work || !(body->zombie_flags & 0x80000000u) ||
        body->pose_owner != work || body->linked_parent != work)
      throw std::runtime_error("Scenario motion selection invalid zombie body");
  }
  root.motions = s.room_motions;
  root.motion_step = steps[selection.speed];
  root.modes = {uint8_t(selection.event_mode ? 5 : 1), selection.motion, 0, 0};
  root.motion_number = (root.motion_number & ~0xffu) | selection.motion;
  root.motion_mode = selection.speed == 1 || selection.speed == 2 ? 0x20 : 0x28;
  root.blend_rate = 0;
  if (body) {
    body->modes[0] = 6;
    body->motion_number = uint32_t(selection.motion) + 1;
    body->blend_count = 8;
    body->motion_mode = root.motion_mode;
    body->motion_step = root.motion_step;
    body->blend_rate = 0;
    body->frame = 0;
    body->frame_direction = 0;
    body->motions = s.room_motions;
  }
  root.control1 = selection.immediate;
  root.control2 = selection.blend > 8 ? 10 : selection.blend;
  if (selection.blend == 6) {
    root.motion_mode |= 8;
    if (body)
      body->blend_count = 0;
  } else if (selection.blend > 8) {
    root.blend_count = selection.blend;
    if (body)
      body->blend_count = selection.blend;
  }
  root.frame = 0;
  const auto action = selection.immediate;
  if (action == 0 || action == 2 || action == 5 || action == 6)
    sample(s, key, 0, 0);
  const bool self = action == 0 || action == 3 || action == 5 || action == 7;
  const bool links = action == 0 || action == 4 || action == 6 || action == 7;
  if (self || links)
    calculate_scenario_models(s, group, work, self, links);
  // Source unmatched immediate actions perform no calculation.
  // Selection does not allocate work. Preserve bound BH_PWORK references while
  // publishing staged scalar/pose/cache changes; validate a record at sampling.
  auto publish = [](auto &destination, auto &replacement) {
    for (size_t i = 0; i < destination.size(); ++i)
      destination[i] = std::move(replacement[i]);
  };
  publish(state.enemies, s.enemies);
  publish(state.objects, s.objects);
  publish(state.items, s.items);
  state.effect_pool = std::move(s.effect_pool);
  state.motion_angle_scratch = s.motion_angle_scratch;
}
void tick_scripted_enemy(ScenarioState &s, size_t work) {
  auto &e = entity(s, {1, work});
  if (!(e.flags & 1) || e.control2 < 0 || e.control2 >= 15)
    throw std::runtime_error("Invalid original scripted enemy callback");
  if (e.modes[3] > 1)
    return; // Original switch has only initialization0 and running1.
  const int index = e.control2;
  const bool moving = index == 1 || index == 3 || index == 7 || index == 9;
  const bool sampling =
      index == 4 || index == 5 || index == 8 || index == 9 || index >= 11;
  if (sampling && e.id == 1) {
    const auto &posed = entity(s, owner(s, {1, work}));
    if (!posed.model_work || posed.joint_pose.size() < 18 ||
        posed.model_work->joints.size() != posed.joint_pose.size())
      throw std::runtime_error(
          "Scripted zombie requires prepared owner pose/cache");
  }
  const auto prior_motion = detail::snapshot(e);
  const auto prior_modes = e.modes;
  const auto prior_direction = e.frame_direction;
  const auto prior_attribute = e.motion_attribute;
  const auto prior_boundary = e.motion_boundary;
  const auto prior_position = e.position;
  const auto prior_angles = e.angles;
  const auto prior_speed = e.speed;
  try {
    if (!e.modes[3]) {
      e.modes[3] = 1;
      e.motion_number = e.modes[1];
      e.frame_direction = 0;
      if (index == 4 || index == 6 || index == 7)
        e.blend_count = 0;
      else if (index == 5)
        e.blend_count = 16;
      else if (index != 10)
        e.blend_count = 8;
      if (index == 4 || index == 5)
        e.motion_step = 65536;
      if (index == 5)
        e.motion_mode |= 0x100;
      if (index == 8 || index >= 11) {
        e.blend_rate = 0;
        e.motion_mode &= ~0x100u;
      } else if (index == 9)
        e.blend_rate = 0;
      if (index == 0 || index == 2 || index == 6 || index == 10)
        e.speed = 0;
    }
    auto position = e.position;
    if (moving) {
      const auto short_value = [](uint32_t bits) {
        const auto value = bits & 0xffff;
        return value < 0x8000 ? int32_t(value) : int32_t(value) - 65536;
      };
      const auto packed = uint32_t(e.control0);
      const float x = short_value(packed >> 16) * .01f;
      const float z =
          (index == 9 ? int32_t(packed & 0xffff) : short_value(packed)) * .01f;
      const float dx = detail::ninja_finite(e.position.x - x);
      const float dz = detail::ninja_finite(e.position.z - z);
      const int32_t angle = int32_t(std::atan2(dx, dz) * 10430.381f);
      const int32_t difference =
          short_value(uint32_t(angle) - uint32_t(e.angles[1]));
      const auto turn = std::max(-182, std::min(182, difference));
      e.angles[1] = signed_word(uint32_t(e.angles[1]) + uint32_t(turn));
      e.speed = detail::ninja_finite(.01f * e.control3);
      // Validate movement before sampling commits pose, then publish afterward
      // to retain source sample-before-AddSpeed order for mv00_subpl1.
      position = player_add_speed(e.position, uint16_t(e.angles[1]), e.speed);
    }
    if (sampling)
      sample(s, {1, work}, signed_word(e.motion_step), e.motion_mode);
    if (moving)
      e.position = position;
  } catch (...) {
    detail::commit(e, prior_motion);
    e.modes = prior_modes;
    e.frame_direction = prior_direction;
    e.motion_attribute = prior_attribute;
    e.motion_boundary = prior_boundary;
    e.position = prior_position;
    e.angles = prior_angles;
    e.speed = prior_speed;
    throw;
  }
}
} // namespace ashford
