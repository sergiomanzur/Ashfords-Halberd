#include "collision_response.hpp"
#include "effects.hpp"
#include "enemy_wall.hpp"
#include "ninja_matrix.hpp"
#include "stateful_wall_internal.hpp"
#include <cstring>
#include <stdexcept>
namespace ashford {
namespace {
float finite(float v) { return detail::ninja_finite(v); }
int32_t increment(int32_t v) {
  const auto bits = uint32_t(v) + 1u;
  return bits < 0x80000000u ? int32_t(bits)
                            : int32_t(int64_t(bits) - 0x100000000ll);
}
float angle_bits(int32_t angle) {
  float v;
  std::memcpy(&v, &angle, 4);
  return v; // POINT offsets alias the source player's int32 angle words.
}
CollisionRecord &wall_at(RoomCollision &room,
                         std::vector<CollisionRecord> &dynamic, size_t index) {
  if (index < room.walls.size())
    return room.walls[index];
  return dynamic.at(index - room.walls.size());
}
} // namespace
StatefulWallResult detail::run_stateful_wall(
    ScenarioState &s, ScenarioEntity &work, ScenarioEntity &player,
    Vec3 proposed, Vec3 previous, float radius, float height, bool wall2,
    const std::function<void(const StatefulWallResult &)> &before_commit) {
  if (!s.collision)
    throw std::runtime_error("Missing stateful room collision");
  auto room = *s.collision;
  auto dynamic = s.dynamic_walls;
  const auto permanent = dynamic.size();
  const bool same = &work == &player;
  const uint32_t saved_flags = work.flags;
  const uint32_t work_flags = wall2 ? saved_flags & ~0x100u : saved_flags;
  uint32_t state_flags = work.state_flags;
  auto remembered = work.step_area, push_index = work.push_index;
  int32_t push_count = work.push_count;
  uint32_t player_flags = same ? work_flags : player.flags;
  auto player_modes = player.modes;
  uint32_t system_flags = s.flags[5][0];
  std::shared_ptr<NativeEffectPool> effects;
  CollisionBody actual;
  actual.position = work.position;
  actual.radius = work.geometry.radius;
  actual.floor = work.floor;
  actual.state_flags = state_flags;
  if (wall2)
    state_flags = check_step_state(room, s.dynamic_miscellaneous, actual);
  if (state_flags & 0x20000) {
    actual.state_flags = state_flags;
    const auto limits =
        build_step_limits(room, s.dynamic_miscellaneous, actual, remembered);
    remembered = limits.remembered_area;
    state_flags = limits.state_flags;
    dynamic.insert(dynamic.end(), limits.walls.begin(), limits.walls.end());
  }
  auto birth = [&](size_t wall) -> bool {
    size_t slot = 0;
    const auto *pool = effects ? effects.get() : s.effect_pool.get();
    if (pool) {
      while (slot < pool->works.size() && (pool->works[slot].flags & 3))
        ++slot;
      if (slot == pool->works.size())
        return false;
    }
    if (!effects)
      effects = s.effect_pool
                    ? std::make_shared<NativeEffectPool>(*s.effect_pool)
                    : std::make_shared<NativeEffectPool>();
    auto &e = effects->works[slot];
    e = {};
    e.flags = 1;
    e.id = 29;
    e.texture_id = -1;
    e.scale = {1, 1, 1};
    e.position = player.position;
    finite(e.position.x);
    finite(e.position.y);
    finite(e.position.z);
    e.offset = {angle_bits(player.angles[0]), angle_bits(player.angles[1]),
                angle_bits(player.angles[2])};
    e.linked_wall = wall;
    // Source sets link80 in bhSetEffect, then clears it immediately. The ATR
    // link remains for Eff029; common unit quad/model dispatch is implicit
    // id29.
    return true;
  };
  WallResponseCallbacks hooks;
  hooks.rectangle_hit = [&](size_t index, Vec3 p) {
    auto &r = wall_at(room, dynamic, index);
    const auto attr = r.attribute;
    if ((attr & 0x10000) && same) {
      const uint32_t yaw = (uint32_t(work.angles[1]) + 8192u) & ~0x3fffu;
      constexpr uint32_t masks[] = {0x40000, 0x200000, 0x100000, 0x80000};
      if (attr & masks[(yaw & 0xc000u) >> 14])
        push_count = 0;
      else {
        const auto sc = ninja_sin_cos(uint16_t(yaw));
        const float x = finite(p.x - 5.f * sc[0]),
                    z = finite(p.z - 5.f * sc[1]);
        if (r.position.x <= x && finite(r.position.x + r.size.x) >= x &&
            r.position.z <= z && finite(r.position.z + r.size.z) >= z)
          push_count = increment(push_count);
        else
          push_count = 0;
      }
    }
    if (!(attr & 0x20000) && (state_flags & 0x80)) {
      r.attribute |= 0x20000;
      push_index = index;
    }
  };
  hooks.rounded_corner = [&] { push_count = 0; };
  hooks.circle_hit = [&](size_t index, Vec3 p) {
    if (player_flags & 6)
      return;
    auto &r = wall_at(room, dynamic, index);
    const float dx = finite(r.position.x - p.x),
                dz = finite(r.position.z - p.z);
    const float squared = finite(dx * dx + dz * dz);
    const float inv = squared ? finite(1.f / std::sqrt(squared)) : 0.f;
    const auto sc = ninja_sin_cos(uint16_t(work.angles[1]));
    const float dot =
        finite(((-sc[0]) * (dx * inv) + 0.f) + (-sc[1]) * (dz * inv));
    player_modes = {2, uint8_t(dot > 0 ? 0 : 1), 0, 0};
    player_flags |= 0x10004;
    system_flags |= 4;
    if (birth(index))
      r.attribute &= ~0x40u;
  };
  CollisionBody body;
  body.position = proposed;
  body.radius = radius;
  body.height = height;
  body.flags = work_flags;
  body.state_flags = state_flags;
  body.floor = work.floor;
  const auto collision =
      collide_walls_ex_context(room, dynamic, body, previous, hooks);
  if (collision.hits)
    state_flags |= 1;
  // Source limits are only available to response, never bhCheckWallType.
  dynamic.resize(permanent);
  StatefulWallResult result;
  result.position = collision.position;
  result.hits = collision.hits;
  result.displacement = {finite(result.position.x - proposed.x),
                         finite(result.position.y - proposed.y),
                         finite(result.position.z - proposed.z)};
  if (wall2 && collision.hits) {
    body.flags = same ? player_flags : work_flags;
    if (const auto *hit = enemy_wall_type(room, dynamic, body)) {
      for (size_t i = 0; i < room.walls.size(); ++i)
        if (&room.walls[i] == hit)
          result.wall = i;
      for (size_t i = 0; i < dynamic.size(); ++i)
        if (&dynamic[i] == hit)
          result.wall = room.walls.size() + i;
    }
  }
  if (before_commit)
    before_commit(result);
  s.collision = std::move(room);
  s.dynamic_walls = std::move(dynamic);
  if (effects)
    s.effect_pool = std::move(effects);
  s.flags[5][0] = system_flags;
  work.state_flags = state_flags;
  work.step_area = remembered;
  work.push_count = push_count;
  work.push_index = push_index;
  player.flags = player_flags;
  player.modes = player_modes;
  if (wall2)
    work.flags = saved_flags;
  return result;
}
StatefulWallResult collide_stateful_walls_ex(ScenarioState &s,
                                             ScenarioEntity &w,
                                             ScenarioEntity &p, Vec3 n, Vec3 o,
                                             float ar, float ah) {
  return detail::run_stateful_wall(s, w, p, n, o, ar, ah, false);
}
StatefulWallResult check_enemy_wall2(ScenarioState &s, ScenarioEntity &w,
                                     ScenarioEntity &p, Vec3 n, Vec3 o,
                                     float ar, float ah) {
  return detail::run_stateful_wall(s, w, p, n, o, ar, ah, true);
}
} // namespace ashford
