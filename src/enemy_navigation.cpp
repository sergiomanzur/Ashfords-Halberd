#include "enemy_navigation_internal.hpp"
#include "ninja_math.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp zonzon.c/hitchk.c stair helpers.
namespace ashford {
namespace {
void finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite enemy stair query value");
}
ScenarioEntity &actor(ScenarioState &state, size_t index) {
  if (state.enemies.size() != 128 || index >= state.enemies.size())
    throw std::runtime_error("Invalid actor stair work slot");
  return state.enemies[index];
}
} // namespace
std::optional<size_t>
enemy_stair_trigger(const RoomCollision &room,
                    const std::vector<CollisionRecord> &dynamic, Vec3 position,
                    int32_t yaw, int32_t floor, float offset) {
  finite(position.x);
  finite(position.z);
  finite(offset);
  const auto rotation = ninja_sin_cos(uint16_t(yaw));
  float px = position.x - offset * rotation[0];
  float pz = position.z - offset * rotation[1];
  finite(px);
  finite(pz);
  auto find = [&](const std::vector<CollisionRecord> &records,
                  size_t base) -> std::optional<size_t> {
    for (size_t i = 0; i < records.size(); ++i) {
      const auto &record = records[i];
      if (!(record.flags & 1) || record.type != 1)
        continue;
      finite(record.position.x);
      finite(record.position.z);
      finite(record.size.x);
      finite(record.size.z);
      const float xmax = record.position.x + record.size.x;
      const float zmax = record.position.z + record.size.z;
      finite(xmax);
      finite(zmax);
      if (record.position.x <= px && xmax >= px && record.position.z <= pz &&
          zmax >= pz && !(record.attribute & 0x400000) &&
          record.floor == floor) {
        // Source reuses probe variables even when alignment rejects this ATR.
        px = .5f + record.position.x;
        pz = .5f + record.position.z;
        const float px2 = xmax - .5f, pz2 = zmax - .5f;
        finite(px);
        finite(pz);
        finite(px2);
        finite(pz2);
        if (record.parameters[1] == 0 || record.parameters[1] == 2) {
          if (px <= position.x && px2 >= position.x)
            return base + i;
        } else if (pz <= position.z && pz2 >= position.z)
          return base + i;
      }
    }
    return std::nullopt;
  };
  if (auto index = find(room.miscellaneous, 0))
    return index;
  return find(dynamic, room.miscellaneous.size());
}
detail::StairPair detail::stair_pair(ScenarioState &state, size_t index) {
  if (!state.collision)
    throw std::runtime_error("Missing current-room stair records");
  auto &room = state.collision->miscellaneous;
  const bool static_record = index < room.size();
  auto &records = static_record ? room : state.dynamic_miscellaneous;
  const size_t local = static_record ? index : index - room.size();
  if (local >= records.size())
    throw std::runtime_error("Actor stair index outside current-room records");
  auto &first = records[local];
  CollisionRecord *second;
  if (first.parameters[3] == 255) {
    const bool forward = first.parameters[0] == 0;
    if ((forward && local + 1 >= records.size()) || (!forward && !local))
      throw std::runtime_error("Adjacent stair pair outside source array");
    second = &records[forward ? local + 1 : local - 1];
  } else {
    if (first.parameters[3] >= room.size())
      throw std::runtime_error(
          "Explicit stair pair outside static room records");
    second = &room[first.parameters[3]];
  }
  return {&first, second};
}
void reserve_actor_stairs(ScenarioState &state, size_t work, size_t index) {
  auto &entity = actor(state, work);
  const auto pair = detail::stair_pair(state, index);
  entity.stair_index = index;
  pair.first->attribute |= 0x400000;
  pair.second->attribute |= 0x400000;
}
void release_actor_stairs(ScenarioState &state, size_t work) {
  const auto &entity = actor(state, work);
  if (!entity.stair_index)
    return;
  const auto pair = detail::stair_pair(state, *entity.stair_index);
  pair.first->attribute &= ~0x400000;
  pair.second->attribute &= ~0x400000;
}
} // namespace ashford
