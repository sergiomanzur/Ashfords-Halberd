#include "collision.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ashford {
namespace {
float finite(float v) {
  if (!std::isfinite(v))
    throw std::runtime_error("Nonfinite ground/water query value");
  return v;
}
bool footprint(const CollisionRecord &r, Vec3 p) {
  const float x = finite(finite(p.x) - finite(r.position.x));
  const float z = finite(finite(p.z) - finite(r.position.z));
  return x >= 0 && x < finite(r.size.x) && z >= 0 && z < finite(r.size.z);
}
const CollisionRecord &merged(const std::vector<CollisionRecord> &first,
                              const std::vector<CollisionRecord> &second,
                              size_t i) {
  return i < first.size() ? first[i] : second[i - first.size()];
}
} // namespace
GroundQuery query_ground_position(const RoomCollision &room,
                                  const std::vector<CollisionRecord> &dynamic,
                                  Vec3 p) {
  GroundQuery result;
  float nearest = 10000;
  for (size_t i = 0; i < room.walls.size() + dynamic.size(); ++i) {
    const auto &r = merged(room.walls, dynamic, i);
    if (!(r.flags & 1))
      continue;
    float height = 0, distance = 0;
    bool inside = false;
    if (r.type == 0 || r.type == 2 || r.type == 4) {
      const float h = finite(r.size.y ? r.size.y : room.height);
      height = finite(finite(r.position.y) + h);
      distance = finite(finite(p.y) - height);
      if (r.type == 0)
        inside = footprint(r, p);
      else if (r.type == 2) {
        const float x = finite(finite(r.position.x) - finite(p.x));
        const float z = finite(finite(r.position.z) - finite(p.z));
        inside = finite(std::sqrt(finite(x * x + z * z))) < finite(r.size.x);
      } else if (height >= r.position.y) {
        const float x = finite(r.position.x), z = finite(r.position.z);
        const float w = finite(r.size.x), d = finite(r.size.z);
        const float end_x = finite(x + w), end_z = finite(z + d);
        const float px = finite(p.x), pz = finite(p.z);
        if (px >= std::min(x, end_x) && px < std::max(x, end_x) &&
            pz >= std::min(z, end_z) && pz < std::max(z, end_z)) {
          // Triangle3's inner division may be infinity at the source origin.
          const float abx = std::abs(d / (w / (px - x)));
          const float abz = finite(std::abs(end_z - pz));
          inside = abz > abx;
        }
      }
      inside = inside && distance >= 0;
    } else if (r.type == 6) {
      if (!footprint(r, p) || finite(finite(p.y) - finite(r.position.y)) < 0)
        continue;
      const float h = finite(r.size.y), y = r.position.y;
      switch (r.id) {
      case 0:
        height = y + (h / r.size.x) * (p.x - r.position.x);
        break;
      case 1:
        height = h + (y + (h / r.size.z) * (r.position.z - p.z));
        break;
      case 2:
        height = y + (h / r.size.z) * (p.z - r.position.z);
        break;
      case 3:
        height = h + (y + (h / r.size.x) * (r.position.x - p.x));
        break;
      default:
        throw std::runtime_error("Unknown source ground slope direction");
      }
      distance = finite(p.y - finite(height));
      inside = true; // Source allows negative nearest distances for slopes.
    } else if (r.type == 7) {
      height = finite(r.position.y);
      distance = finite(finite(p.y) - height);
      if (distance <= 0 && distance >= -.0001f)
        distance = 0;
      inside = footprint(r, p) && distance >= 0;
    }
    if (inside && distance < nearest) {
      nearest = distance;
      result = {height, i};
    }
  }
  return result;
}
std::optional<size_t> query_water(const RoomCollision &room,
                                  const std::vector<CollisionRecord> &dynamic,
                                  Vec3 p) {
  for (size_t i = 0; i < room.floors.size() + dynamic.size(); ++i) {
    const auto &r = merged(room.floors, dynamic, i);
    if (!(r.flags & 1) || r.type != 3 || r.parameters[0] != 1)
      continue;
    if (finite(r.position.x) <= finite(p.x) &&
        finite(r.position.x + finite(r.size.x)) >= p.x &&
        finite(r.position.z) <= finite(p.z) &&
        finite(r.position.z + finite(r.size.z)) >= p.z &&
        finite(r.position.y) <= finite(p.y) &&
        finite(r.position.y + finite(r.size.y)) >= p.y)
      return i;
  }
  return {};
}
} // namespace ashford
