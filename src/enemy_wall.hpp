#pragma once
#include "collision.hpp"
namespace ashford {
// hitchk.c::bhCheckWallType: first static/dynamic hit, without response.
// Returned pointers remain owned by the supplied stable vectors.
const CollisionRecord *
enemy_wall_type(const RoomCollision &room,
                const std::vector<CollisionRecord> &dynamic,
                const CollisionBody &body);
// zonzon.c::CheckDirWall3/SideWall3. The supplied body position is the probe
// origin; callers of the source *2 variants add only their XZ offset first.
const CollisionRecord *enemy_direction_wall(
    const RoomCollision &room, const std::vector<CollisionRecord> &dynamic,
    CollisionBody body, int32_t yaw, int32_t relative, float step);
int enemy_side_wall(const RoomCollision &room,
                    const std::vector<CollisionRecord> &dynamic,
                    CollisionBody body, int32_t yaw, float step, int both);
} // namespace ashford
