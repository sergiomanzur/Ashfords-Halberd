// Native shared-pool shadow creation/control from MIT recvx-decomp
// effect.c::bhSetShadow, event.c::bhInitSetKage, effsub1.c::bhEff001.
#include "effects.hpp"
#include "ninja_matrix.hpp"
#include "scenario.hpp"
#include <algorithm>
#include <stdexcept>
namespace ashford {
namespace {
Vec3 finite(Vec3 v) {
  detail::ninja_finite(v.x);
  detail::ninja_finite(v.y);
  detail::ninja_finite(v.z);
  return v;
}
const ActorModelWork &cached(const ScenarioState &s, size_t work) {
  const auto owner = s.enemies[work].pose_owner.value_or(work);
  if (owner >= s.enemies.size() || !s.enemies[owner].model_work ||
      (owner != work && s.enemies[owner].pose_owner))
    throw std::runtime_error("Missing native shadow model owner");
  return *s.enemies[owner].model_work;
}
detail::NinjaMatrix joint(const ActorModelWork &cache, int32_t index) {
  if (index < 0 || size_t(index) >= cache.joints.size())
    throw std::runtime_error("Native shadow joint outside cached model");
  return detail::ninja_load(cache.joints[size_t(index)].matrix);
}
} // namespace
std::optional<size_t> create_enemy_shadow(ScenarioState &s, size_t work,
                                          const EnemyShadow &shadow) {
  if (work >= s.enemies.size() || work >= 128 || shadow.anchor_joint < 0 ||
      shadow.joints.size() > 15)
    throw std::runtime_error("Invalid native shadow creation request");
  finite(shadow.size);
  size_t slot = 0;
  while (slot < 512 && s.effect_pool && (s.effect_pool->works[slot].flags & 3))
    ++slot;
  if (slot == 512)
    return {};
  auto pool = s.effect_pool ? std::make_shared<NativeEffectPool>(*s.effect_pool)
                            : std::make_shared<NativeEffectPool>();
  NativeEffectWork e;
  e.flags = 0x240081;
  e.id = 1;
  e.type = shadow.joints.empty() ? 1 : 0;
  e.texture_id = -1;
  e.scale = e.base_scale = shadow.size;
  e.linked_parent = work;
  e.linked_joint = shadow.anchor_joint;
  if (shadow.joints.empty())
    e.shadow_joints[0] = -1;
  else
    for (size_t i = 0; i < shadow.joints.size(); ++i) {
      e.shadow_joints[i] = shadow.joints[i];
      if (shadow.joints[i] == -1)
        break;
    }
  pool->works[slot] = std::move(e);
  s.effect_pool = std::move(pool);
  return slot;
}
void request_enemy_shadow(ScenarioState &s, size_t authored, uint8_t variant) {
  if (authored >= s.enemy_slots.size() ||
      s.enemy_slots[authored] >= s.enemies.size())
    throw std::runtime_error(
        "Native shadow request outside authored enemy map");
  const auto slot = s.enemy_slots[authored];
  auto &e = s.enemies[slot];
  if (e.flags & 0x800)
    return;
  if (variant <= 4)
    create_enemy_shadow(
        s, slot,
        EnemyShadow{
            {}, 1, variant == 4 ? Vec3{6.5f, 4, 4.5f} : Vec3{4.5f, 4, 3.5f}});
  // Source sets the gate even for an exhausted pool or an unmatched variant.
  e.flags |= 0x800;
}
void tick_shadow_effects(ScenarioState &s) {
  if ((!(s.flags[6][0] & 8) && !(s.effect_flags & 1)) || !s.effect_pool)
    return;
  bool active = false;
  for (const auto &e : s.effect_pool->works)
    if ((e.flags & 1) && !(e.state_flags & 0x1000000) && e.id == 1)
      active = true;
  if (!active)
    return;
  auto pool = std::make_shared<NativeEffectPool>(*s.effect_pool);
  for (auto &e : pool->works) {
    if (!(e.flags & 1) || (e.state_flags & 0x1000000) || e.id != 1)
      continue;
    if (!e.linked_parent || *e.linked_parent >= s.enemies.size() ||
        e.modes[0] > 1 || e.type > 1)
      throw std::runtime_error("Unsupported native shadow owner/controller");
    const auto work = *e.linked_parent;
    const auto &parent = s.enemies[work];
    const ActorModelWork *model = nullptr;
    if (parent.flags & 1) {
      model = &cached(s, work);
      if (e.flags & 0x80)
        e.position =
            detail::ninja_point(joint(*model, e.linked_joint), e.offset);
    } else if (e.modes[0] == 1) {
      model = &cached(s, work);
    }
    if (!e.modes[0]) {
      e.flags |= 0x8000000;
      e.modes[0] = 1;
      continue;
    }
    if (!model)
      throw std::runtime_error("Missing active native shadow model");
    e.angles[1] = parent.angles[1];
    if (!e.model_version) {
      if (parent.state_flags & 0x2000)
        throw std::runtime_error(
            "Native shadow ground-cache branch requires original O_WORK pyb");
      if (!s.collision)
        throw std::runtime_error("Missing native shadow room ground");
      const auto ground =
          query_ground_position(*s.collision, s.dynamic_walls, e.position);
      e.position.y = ground.height;
      pool->ground_wall = ground.wall;
    }
    if (e.type == 0) {
      float minx = 100, minz = 100, maxx = -100, maxz = -100;
      auto inverse = detail::ninja_identity();
      detail::ninja_rotate(
          inverse, {0, int32_t(uint16_t(0u - uint32_t(e.angles[1]))), 0});
      for (const auto j : e.shadow_joints) {
        if (j == -1)
          break;
        const auto m = joint(*model, j);
        const Vec3 relative{detail::ninja_finite(m[3] - e.position.x), 0,
                            detail::ninja_finite(m[11] - e.position.z)};
        const auto point = detail::ninja_point(inverse, relative);
        minx = std::min(minx, point.x);
        minz = std::min(minz, point.z);
        maxx = std::max(maxx, point.x);
        maxz = std::max(maxz, point.z);
      }
      float x = detail::ninja_finite(.8f * (maxx - minx)),
            z = detail::ninja_finite(.8f * (maxz - minz));
      const auto base = finite(e.base_scale);
      if (x < base.x)
        x = base.x;
      if (z < base.z)
        z = base.z;
      const float limit_x = detail::ninja_finite(2 * base.x),
                  limit_z = detail::ninja_finite(2 * base.z);
      if (x > limit_x)
        x = limit_x;
      if (z > limit_z)
        z = limit_z;
      e.scale.x = x;
      e.scale.z = z;
    }
    if (!(parent.flags & 1)) {
      e.flags = 0;
      continue;
    }
    const bool hidden =
        bool(parent.state_flags & 0x41000008) || bool(parent.model_flags & 1);
    if ((hidden && !(s.shadow_flags & 1)) ||
        (!(parent.state_flags & 0x40000000) && !(s.shadow_flags & 2)))
      e.flags |= 0x1000000;
    else
      e.flags &= ~0x1000000u;
  }
  s.effect_pool = std::move(pool);
}
} // namespace ashford
