#pragma once
#include "scene.hpp"
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
