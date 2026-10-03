#pragma once
#include "scene.hpp"
#include <optional>
namespace ashford {
struct CollisionRecord {
  uint8_t flags = 0, type = 0, id = 0;
  int8_t floor = 0;
  uint32_t attribute = 0;
  Vec3 position, size;
  std::array<uint8_t, 4> parameters{};
};
struct RoomCollision {
  std::vector<CollisionRecord> walls, miscellaneous, floors;
  float height = 0;
  std::array<float, 32> ground{};
};
RoomCollision decode_room_collision(const Bytes &data);
int8_t player_floor_number(const RoomCollision &room, float height);
bool player_in_water(const RoomCollision &room, Vec3 position);
struct GroundQuery {
  float height = -1000;
  // Original static-then-dynamic wall index (sys->htp).
  std::optional<size_t> wall;
};
struct LineCollisionResult {
  Vec3 position{}, normal{};
  // bhCollisionCheckLine: last successful static-then-dynamic wall index.
  std::optional<size_t> wall;
};
LineCollisionResult
query_collision_line(const RoomCollision &room,
                     const std::vector<CollisionRecord> &dynamic, Vec3 from,
                     Vec3 to);
GroundQuery query_ground_position(const RoomCollision &room,
                                  const std::vector<CollisionRecord> &dynamic,
                                  Vec3 position);
// bhCheckWater: first inclusive flags1/type3/prm0=1 static or dynamic floor.
std::optional<size_t> query_water(const RoomCollision &room,
                                  const std::vector<CollisionRecord> &dynamic,
                                  Vec3 position);
struct CollisionBody {
  Vec3 position;
  float radius = 3.5f, height = 16.5f;
  int8_t floor = 0;
  uint32_t flags = 0, state_flags = 0;
};
struct CollisionResult {
  Vec3 position;
  size_t hits = 0;
  bool reset_push_count = false;
  int8_t floor = 0;
};
// Static response from hitchk.c::bhCheckWall, including triangles, slopes and
// ledges. Dynamic walls, step limits and special damage need gameplay state.
CollisionResult collide_player_walls(const RoomCollision &room,
                                     const CollisionBody &body);
// bhCheckWallEx uses the prior position for rectangle/ledge quadrant selection
// and rectangle face selection, retains raw heights, and skips slope records.
CollisionResult collide_player_walls_ex(const RoomCollision &room,
                                        const CollisionBody &body,
                                        Vec3 previous);
} // namespace ashford
