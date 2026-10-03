#include "collision.hpp"
#include <cmath>
#include <stdexcept>
namespace ashford {
namespace {
float finite(float v) {
  if (!std::isfinite(v))
    throw std::runtime_error("Nonfinite consumed step geometry");
  return v;
}
const CollisionRecord &area_at(const RoomCollision &room,
                               const std::vector<CollisionRecord> &dynamic,
                               size_t index) {
  if (index < room.miscellaneous.size())
    return room.miscellaneous[index];
  index -= room.miscellaneous.size();
  if (index >= dynamic.size())
    throw std::runtime_error("Invalid remembered step area index");
  return dynamic[index];
}
} // namespace
std::optional<size_t>
query_step_area(const RoomCollision &room,
                const std::vector<CollisionRecord> &dynamic, int8_t floor,
                float x, float z) {
  finite(x);
  finite(z);
  for (size_t i = 0; i < room.miscellaneous.size() + dynamic.size(); ++i) {
    const auto &r = area_at(room, dynamic, i);
    if (!(r.flags & 1) || r.type != 2 || !r.parameters[0] || r.floor != floor)
      continue;
    const float rx = finite(r.position.x), rz = finite(r.position.z);
    const float ex = finite(rx + finite(r.size.x)),
                ez = finite(rz + finite(r.size.z));
    if (rx <= x && ex >= x && rz <= z && ez >= z)
      return i;
  }
  return {};
}
uint32_t check_step_state(const RoomCollision &room,
                          const std::vector<CollisionRecord> &dynamic,
                          const CollisionBody &body) {
  const uint32_t flags = body.state_flags & ~0x20000u;
  if (!(flags & 0x30) && query_step_area(room, dynamic, body.floor,
                                         body.position.x, body.position.z))
    return flags | 0x20000u;
  return flags;
}
StepLimitResult build_step_limits(const RoomCollision &room,
                                  const std::vector<CollisionRecord> &dynamic,
                                  const CollisionBody &body,
                                  std::optional<size_t> remembered) {
  const float x = finite(body.position.x), z = finite(body.position.z);
  const float ar = finite(body.radius);
  if (ar <= 0)
    throw std::runtime_error("Invalid step boundary radius");
  const float north = finite(z - ar), east = finite(x + ar);
  const float south = finite(z + ar), west = finite(x - ar), tmp = .5f * ar;
  const auto current = query_step_area(room, dynamic, body.floor, x, z);
  StepLimitResult result;
  result.state_flags = body.state_flags;
  result.remembered_area = current ? current : remembered;
  if (!result.remembered_area) {
    result.state_flags &= ~0x20000u;
    return result;
  }
  // Source uses the old pointer directly when the center leaves every area;
  // do not re-filter its flags/floor or inspect a stale index if current wins.
  const auto &pop = area_at(room, dynamic, *result.remembered_area);
  const std::array<Vec3, 4> probes = {Vec3{x, 0, north}, Vec3{east, 0, z},
                                      Vec3{x, 0, south}, Vec3{west, 0, z}};
  for (size_t side = 0; side < probes.size(); ++side) {
    if (query_step_area(room, dynamic, body.floor, probes[side].x,
                        probes[side].z))
      continue;
    const float px = finite(pop.position.x), py = finite(pop.position.y);
    const float pz = finite(pop.position.z);
    const float twice = finite(2.f * ar), four = finite(4.f * ar);
    CollisionRecord wall;
    wall.flags = 1;
    wall.type = 1;
    wall.floor = body.floor;
    wall.attribute = 0x800000;
    // ID is not authored by this source helper and is unused for type1.
    wall.position.y = py;
    wall.size.y = 1.f;
    switch (side) {
    case 0:
      wall.position.x = finite(px - twice);
      wall.position.z = finite(pz - finite(twice + tmp));
      wall.size.x = finite(finite(pop.size.x) + four);
      wall.size.z = twice;
      break;
    case 1:
      wall.position.x = finite(tmp + finite(px + finite(pop.size.x)));
      wall.position.z = finite(pz - twice);
      wall.size.x = twice;
      wall.size.z = finite(finite(pop.size.z) + four);
      break;
    case 2:
      wall.position.x = finite(px - twice);
      wall.position.z = finite(tmp + finite(pz + finite(pop.size.z)));
      wall.size.x = finite(finite(pop.size.x) + four);
      wall.size.z = twice;
      break;
    case 3:
      wall.position.x = finite(px - finite(twice + tmp));
      wall.position.z = finite(pz - twice);
      wall.size.x = twice;
      wall.size.z = finite(finite(pop.size.z) + four);
      break;
    }
    result.walls.push_back(wall);
  }
  return result;
}
} // namespace ashford
