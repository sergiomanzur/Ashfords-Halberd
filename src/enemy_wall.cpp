#include "enemy_wall.hpp"
#include "collision_shapes.hpp"
#include "ninja_math.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp hitchk.c and zonzon.c wall queries.
namespace ashford {
namespace {
float finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite enemy wall query");
  return value;
}
} // namespace
const CollisionRecord *
enemy_wall_type(const RoomCollision &room,
                const std::vector<CollisionRecord> &dynamic,
                const CollisionBody &body) {
  const auto p = body.position;
  finite(p.x);
  finite(p.y);
  finite(p.z);
  const float radius = body.radius, top = finite(p.y + finite(body.height));
  auto find = [&](const std::vector<CollisionRecord> &records)
      -> const CollisionRecord * {
    for (const auto &r : records) {
      if (!(r.flags & 1) || r.type > 7)
        continue;
      if (r.type < 6 && (r.type & 1) && (body.flags & 0x400))
        continue;
      if (r.type != 6)
        finite(radius);
      const float x = finite(r.position.x), y = finite(r.position.y),
                  z = finite(r.position.z);
      const float w = finite(r.size.x), h = finite(r.size.y);
      if (r.type == 4 || r.type == 5) {
        finite(r.size.z);
        if (!h)
          finite(room.height);
        if (triangle_wall_inside(r, p, radius, body.height, room.height))
          return &r;
        continue;
      }
      if (r.type == 2 || r.type == 3) {
        const float dx = finite(p.x - x), dz = finite(p.z - z);
        const float length = std::sqrt(finite(dx * dx + dz * dz));
        if (!(length < finite(radius + w)))
          continue;
      } else {
        const float expansion = r.type == 6 ? 0 : radius;
        const float dx = finite(p.x - finite(x - expansion));
        const float dz = finite(p.z - finite(z - expansion));
        const float width = finite(w + 2.f * expansion);
        const float depth = finite(finite(r.size.z) + 2.f * expansion);
        if (!(dx >= 0 && dx < width && dz >= 0 && dz < depth))
          continue;
      }
      if (r.type == 7) {
        const float edge = finite(y + h);
        if (body.flags & 0x100) {
          if (p.y < edge && top > edge)
            return &r;
        } else if (top >= edge && p.y < y)
          return &r;
      } else if (top >= y && p.y <= finite(y + (h ? h : finite(room.height))))
        return &r;
    }
    return nullptr;
  };
  if (auto hit = find(room.walls))
    return hit;
  return find(dynamic);
}
const CollisionRecord *enemy_direction_wall(
    const RoomCollision &room, const std::vector<CollisionRecord> &dynamic,
    CollisionBody body, int32_t yaw, int32_t relative, float step) {
  finite(step);
  const auto trig = ninja_sin_cos(uint16_t(uint32_t(yaw) + uint32_t(relative)));
  body.position.x -= step * trig[0];
  body.position.z -= step * trig[1];
  return enemy_wall_type(room, dynamic, body);
}
int enemy_side_wall(const RoomCollision &room,
                    const std::vector<CollisionRecord> &dynamic,
                    CollisionBody body, int32_t yaw, float step, int both) {
  const bool left =
      enemy_direction_wall(room, dynamic, body, yaw, -16384, step);
  const bool right =
      enemy_direction_wall(room, dynamic, body, yaw, 16384, step);
  return left && right ? both : int(left) - int(right);
}
} // namespace ashford
