#pragma once
#include "locomotion.hpp"
namespace ashford {
struct RoomPosition {
  Vec3 position;
  uint32_t yaw = 0;
};
// ETTY_WORK disk records. These describe authored entities; runtime scripts
// still determine visibility, activation and behavior.
struct RoomEntity {
  uint32_t flags = 0;
  uint16_t id = 0, type = 0;
  int8_t floor = 0, parameter = 0;
  uint8_t model_version = 0, work_number = 0;
  Vec3 position;
  int16_t rotation_x = 0, rotation_z = 0, rotation_y = 0, angular_speed = 0;
  std::array<int8_t, 4> hide{};
};
struct RoomRecords {
  std::vector<RoomPosition> positions;
  std::vector<RoomEntity> enemies, objects, items;
};
RoomRecords decode_room_records(const Bytes &data);
// Normal door entry from room.c::bhSetRoom and bhInitRoomChangePlayer.
// Save restoration and scripted introduction entry use separate paths.
PlayerWorldState enter_room(const RoomRecords &records, size_t position,
                            const RoomCollision &collision,
                            const std::vector<RoomCut> &cuts);
} // namespace ashford
