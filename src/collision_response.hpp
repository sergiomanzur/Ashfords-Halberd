#pragma once
#include "collision.hpp"
#include <functional>
namespace ashford::detail {
struct WallResponseCallbacks {
  std::function<void(size_t, Vec3)> rectangle_hit, circle_hit;
  std::function<void()> rounded_corner;
};
CollisionResult collide_walls_ex_context(
    const RoomCollision &, const std::vector<CollisionRecord> &dynamic,
    const CollisionBody &, Vec3 previous, const WallResponseCallbacks &);
} // namespace ashford::detail
