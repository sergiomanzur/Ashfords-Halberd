#include "effects.hpp"
#include "ninja_matrix.hpp"
#include "zombie.hpp"
#include "zombie_effect_tables.hpp"
#include <limits>
#include <stdexcept>
// Native creation from en01.c, zonzon.c, effect.c, effsub3.c and pwksub.c.
// Playback callbacks run later in the source; birth does not consume RNG.
namespace ashford {
namespace {
ScenarioEntity &enemy_work(ScenarioState &s, size_t work) {
  if (s.enemies.size() != 128 || work >= s.enemies.size() ||
      !(s.enemies[work].flags & 1) || s.enemies[work].id != 1)
    throw std::runtime_error("Invalid zombie effect work");
  return s.enemies[work];
}
std::shared_ptr<NativeEffectPool> stage(const ScenarioState &s) {
  return s.effect_pool ? std::make_shared<NativeEffectPool>(*s.effect_pool)
                       : std::make_shared<NativeEffectPool>();
}
std::optional<size_t> allocate(NativeEffectPool &pool, size_t &cursor) {
  for (size_t i = cursor; i < pool.works.size(); ++i) {
    if (!(pool.works[i].flags & 3)) {
      pool.works[i] = {};
      pool.works[i].flags = 2;
      cursor = i + 1;
      return i;
    }
  }
  return {};
}
Vec3 finite(Vec3 v) {
  detail::ninja_finite(v.x);
  detail::ninja_finite(v.y);
  detail::ninja_finite(v.z);
  return v;
}
float cached_float(double value) {
  if (!std::isfinite(value) ||
      std::abs(value) > std::numeric_limits<float>::max())
    throw std::runtime_error("Effect cached coordinate outside float range");
  return float(value);
}
size_t texture_owner(const ScenarioState &s, size_t work) {
  const size_t owner = s.enemies[work].pose_owner.value_or(work);
  if (owner >= s.enemies.size() || !(s.enemies[owner].flags & 1) ||
      (owner != work && s.enemies[owner].pose_owner) ||
      !s.enemies[owner].model_work)
    throw std::runtime_error("Missing effect model/texture owner");
  return owner;
}
const ModelMatrix &joint(const ScenarioState &s, size_t work, int index) {
  const size_t owner = texture_owner(s, work);
  if (index < 0 || size_t(index) >= s.enemies[owner].model_work->joints.size())
    throw std::runtime_error("Missing cached effect joint");
  return s.enemies[owner].model_work->joints[index].matrix;
}
std::optional<size_t> table_effect(NativeEffectPool &pool,
                                   std::optional<size_t> parent,
                                   int joint_index, Vec3 offset = {}) {
  size_t cursor = 0;
  const auto slot = allocate(pool, cursor);
  if (!slot)
    return {}; // Source exhaustion does not consume initializer values.
  auto &w = pool.works[*slot];
  const auto &ef = pool.scratch;
  w.flags = ef.flags;
  w.id = ef.id;
  w.type = ef.type;
  w.model_version = uint8_t(ef.model_version);
  const uint8_t floor = uint8_t(ef.floor);
  w.floor = int8_t(floor < 128 ? int(floor) : int(floor) - 256);
  w.position = finite(ef.position);
  w.scale = finite(ef.scale);
  w.base_scale = w.scale;
  w.angles = {ef.angles[0], ef.angles[1], 0};
  w.texture_id = -1;
  w.offset = finite(offset);
  w.linked_parent = parent;
  w.linked_joint = joint_index;
  if (parent)
    w.flags |= 0x80;
  return slot;
}
int texture_id(uint8_t model, int base) {
  switch (model) {
  case 2:
  case 3:
  case 9:
  case 10:
  case 32:
  case 33:
  case 39:
  case 43:
    return base;
  case 4:
  case 21:
  case 34:
    return base + 1;
  case 7:
  case 8:
  case 31:
  case 37:
  case 38:
  case 42:
    return base + 2;
  case 0:
  case 1:
  case 5:
  case 6:
  case 22:
  case 35:
  case 36:
    return base + 3;
  case 40:
    return base + 4;
  default:
    return base + 5;
  }
}
void model_texture_effect(NativeEffectPool &pool, const ScenarioState &s,
                          size_t work, std::optional<size_t> slot, int texture,
                          Vec3 direction) {
  if (!slot)
    return;
  auto &w = pool.works[*slot];
  w.texture_owner = texture_owner(s, work);
  w.state_flags |= 0x20;
  w.texture_id = texture;
  w.direction = finite(direction);
}
void ring(NativeEffectPool &pool, const ScenarioState &s, size_t work,
          int size) {
  const auto &m = joint(s, work, 0);
  size_t cursor = 0;
  const auto slot = allocate(pool, cursor);
  if (!slot)
    return;
  auto &w = pool.works[*slot];
  w.flags = 1;
  w.id = 108;
  w.texture_id = -1;
  w.position = {cached_float(m[3]), cached_float(m[7]), cached_float(m[11])};
  w.scale = {1, 1, 1};
  w.linked_joint = size;
  // POINT's trailing words alias unrelated cache storage; effect108 uses XYZ.
}
void blood_particles(NativeEffectPool &pool, size_t work, Vec3 direction) {
  size_t cursor =
      0; // AllocOwork resets once per particle birth, not per group.
  for (uint32_t level : {255u, 224u}) {
    const auto main = allocate(pool, cursor), a = allocate(pool, cursor),
               b = allocate(pool, cursor);
    if (!main || !a || !b) {
      for (auto slot : {main, a, b})
        if (slot)
          pool.works[*slot].flags = 0;
      return;
    }
    auto &w = pool.works[*main];
    w.flags = 0x81;
    w.id = 302;
    w.type = 1; // PtclDat01: mov_no1, col_lv255/224, drw_typ1.
    w.linked_parent = work;
    w.linked_joint = 11;
    w.offset = {0, 0, -1.8f};
    w.texture_slot = 4;
    w.particle_sprite = true;
    w.particle_storage = std::array<size_t, 2>{*a, *b};
    constexpr uint32_t color = 0xff5f0000;
    w.motion_color = ((((color & 0xff00ff) * (level + 1)) >> 8) & 0xff00ff) |
                     ((((color & 0xff00) * (level + 1)) >> 8) & 0xff00) |
                     0xff000000;
    w.motion_number = (w.motion_color & 0xffffff) | 0x20000000;
    const float inv =
        1 / std::sqrt((direction.x * direction.x + direction.y * direction.y) +
                      direction.z * direction.z);
    w.direction =
        finite({direction.x * inv, direction.y * inv, direction.z * inv});
  }
}
void water_splash(NativeEffectPool &pool, const ScenarioState &s,
                  const ScenarioEntity &e, Vec3 point, float scale) {
  if (!s.collision)
    throw std::runtime_error("Water splash requires room collision");
  Vec3 position{point.x, e.position.y, point.z};
  const auto ground =
      query_ground_position(*s.collision, s.dynamic_walls, position);
  pool.ground_wall = ground.wall;
  position.y = ground.height;
  const auto water = query_water(*s.collision, s.dynamic_floors, position);
  if (!water)
    return;
  const auto &floor =
      *water < s.collision->floors.size()
          ? s.collision->floors[*water]
          : s.dynamic_floors[*water - s.collision->floors.size()];
  auto &ef = pool.scratch;
  ef.id = 11;
  ef.flags = 1;
  ef.model_version = 0;
  ef.type = 1;
  ef.floor = 0;
  ef.scale = {scale, scale, scale};
  ef.position = {point.x, detail::ninja_finite(floor.position.y + floor.size.y),
                 point.z};
  int index = 0;
  for (int offset : {16384, -16384, 32768}) {
    const int64_t angle = int64_t(e.angles[1]) + offset;
    const uint16_t low = uint16_t(angle);
    ef.angles[1] = int16_t(low < 32768 ? int(low) : int(low) - 65536);
    table_effect(pool, {}, index++);
  }
}
} // namespace
void tick_zombie_kami_effect(ScenarioState &s, size_t work, int32_t frame) {
  auto &e = enemy_work(s, work);
  for (const auto &event : detail::en01_keff_tbl) {
    if (event.motion == -1)
      return;
    if (e.motion_number != uint32_t(event.motion))
      continue;
    for (size_t i = 0; i < 4; ++i) {
      if (frame != event.frames[i])
        continue;
      auto pool = stage(s);
      auto &ef = pool->scratch;
      ef.flags = 1;
      ef.position = {};
      ef.scale = {1.5f, 1.5f, 0};
      ef.angles[1] = 0;
      ef.model_version = 0;
      ef.id = 5;
      ef.type = 1;
      table_effect(*pool, work, 11, {0, 0, -1.8f});
      Vec3 velocity = e.damage_velocity;
      if (i == 0) {
        const auto yaw = ninja_sin_cos(uint16_t(e.angles[1]));
        velocity = {-yaw[0], ninja_sin_cos(10922)[0], -yaw[1]};
        blood_particles(*pool, work, velocity);
      }
      s.effect_pool = std::move(pool);
      e.damage_joint = 11;
      e.damage_velocity = velocity;
      return; // Kami frame-1 entries are literal matches, not terminators.
    }
  }
}
void tick_zombie_water_effect(ScenarioState &s, size_t work, int32_t frame) {
  const auto &e = enemy_work(s, work);
  if (!(e.state_flags & 0x100000))
    return;
  std::shared_ptr<NativeEffectPool> pool;
  for (const auto &event : detail::en01_weff_tbl) {
    if (event.motion == -1)
      break;
    if (e.motion_number != uint32_t(event.motion))
      continue;
    for (const auto &step : event.steps) {
      if (step.frame == -1)
        break;
      if (step.frame != frame)
        continue;
      if (!pool)
        pool = stage(s);
      if (step.size != -1)
        ring(*pool, s, work, step.size);
      if (step.joint != -1) {
        auto matrix = detail::ninja_identity();
        detail::ninja_rotate(matrix, {0, e.angles[1], 0});
        const auto offset = detail::ninja_point(matrix, {step.x, 0, step.z});
        const auto &m = joint(s, work, step.joint);
        const auto point = finite(
            {cached_float(m[3]) - offset.x, 0, cached_float(m[11]) - offset.z});
        const float scale = step.size < 11 ? .8f : step.size < 13 ? 1.4f : 2;
        water_splash(*pool, s, e, point, scale);
      }
    }
  }
  if (pool)
    s.effect_pool = std::move(pool);
}
ZombieMotionEffects native_zombie_motion_effects() {
  return {tick_zombie_kami_effect, tick_zombie_water_effect};
}
void tick_zombie_neck_blood_effect(ScenarioState &s, size_t work, int type) {
  // Source nonzero types do not consume any work or template fields.
  if (type != 0)
    return;
  const auto &e = enemy_work(s, work);
  auto pool = stage(s);
  for (int i = 0; i < 4; ++i) {
    const auto yaw = ninja_sin_cos(uint16_t(8192 + i * 16384));
    for (int angle : {10240, 4096}) {
      const bool upper = angle == 10240;
      const auto elevation = ninja_sin_cos(uint16_t(angle));
      const Vec3 direction = {-yaw[0] * elevation[1], elevation[0],
                              -yaw[1] * elevation[1]};
      auto &ef = pool->scratch;
      ef.id = 370;
      ef.flags = 1;
      ef.type = upper ? 1 : 3;
      ef.scale = {3.5f, 3.5f, 0};
      ef.position = {};
      const auto slot = table_effect(*pool, work, 10);
      model_texture_effect(*pool, s, work, slot, texture_id(e.model_version, 6),
                           direction);
      if (slot) {
        pool->works[*slot].control0 = i;
        pool->works[*slot].control1 = upper ? 6 : 3;
      }
    }
  }
  s.effect_pool = std::move(pool);
}
void tick_zombie_head_fragment_effect(ScenarioState &s, size_t work, int) {
  const auto &e = enemy_work(s, work);
  const auto point =
      detail::ninja_point(detail::ninja_load(joint(s, work, 10)), {0, .5f, -1});
  auto pool = stage(s);
  auto seed = s.random_seed;
  for (int i = 0; i < 4; ++i) {
    const auto yaw = ninja_sin_cos(
        uint16_t(uint32_t(e.angles[1]) + 8192u + uint32_t(i) * 16384u));
    const auto elevation = ninja_sin_cos(4096);
    const Vec3 direction = {-yaw[0] * elevation[1], elevation[0],
                            -yaw[1] * elevation[1]};
    auto &ef = pool->scratch;
    ef.flags = 1;
    ef.id = 362;
    ef.type = 0;
    ef.position = point;
    // USA rand's wrapped LCG; source consumes it before allocation.
    seed = seed * 1103515245u + 12345u;
    const int32_t roll = int32_t(seed & 0x7fffffffu);
    ef.scale = {2.f + float(-roll) / -2.1474836E9f, 0, 0};
    ef.scale.y = ef.scale.x;
    const auto slot = table_effect(*pool, {}, 0);
    model_texture_effect(*pool, s, work, slot, texture_id(e.model_version, 5),
                         direction);
  }
  s.effect_pool = std::move(pool);
  s.random_seed = seed;
}
} // namespace ashford
