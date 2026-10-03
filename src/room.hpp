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
// On-disc EF_WRK is68 bytes; signed fields and console link indices are values.
struct RoomEffectRecord {
  uint32_t flags = 0;
  uint16_t id = 0, type = 0, model_version = 0;
  int16_t floor = 0;
  Vec3 position{}, scale{}, offset{};
  std::array<int16_t, 2> angles{};
  int32_t link_type = 0, link_index = 0, linked_joint = 0, parameter = 0;
};
std::vector<RoomEffectRecord> decode_room_effects(const Bytes &data);
struct RoomRoutes {
  std::vector<CollisionRecord> regions;
  Bytes next_hops;
};
RoomRoutes decode_room_routes(const Bytes &data);
// rutchk.c: inclusive XYZ region membership ignores ATR flags. A missing
// source/destination region returns0xff without changing output; same-region
// copies the destination and returns0xff. Other queries return the next hop.
uint8_t room_route_id(const RoomRoutes &routes, Vec3 position);
uint8_t room_route(const RoomRoutes &routes, Vec3 source, Vec3 destination,
                   Vec3 &output);
// Original ps2_dvd_image.c::rdx_files ordering. Missing variants are errors,
// rather than silently loading another room case.
size_t room_archive_index(uint8_t stage, uint8_t room, uint8_t room_case);
// Normal door entry from room.c::bhSetRoom and bhInitRoomChangePlayer.
// Save restoration and scripted introduction entry use separate paths.
PlayerWorldState enter_room(const RoomRecords &records, size_t position,
                            const RoomCollision &collision,
                            const std::vector<RoomCut> &cuts);
} // namespace ashford
