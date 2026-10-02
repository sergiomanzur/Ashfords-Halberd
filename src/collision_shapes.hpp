#pragma once
#include "collision.hpp"
namespace ashford {
bool triangle_wall_inside(const CollisionRecord &record, Vec3 position,
                          float radius, float height, float room_height);
}
