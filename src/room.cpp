// Native disk decoding and normal room entry from MIT recvx-decomp room.c.
#include "room.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ashford {
namespace {
void range(const Bytes &b, size_t p, size_t n) {
  if (p > b.size() || n > b.size() - p)
    throw std::runtime_error("Room record outside data");
}
uint32_t u32(const Bytes &b, size_t p) {
  range(b, p, 4);
  return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
         uint32_t(b[p + 3]) << 24;
}
int16_t s16(const Bytes &b, size_t p) {
  range(b, p, 2);
  int v = int(b[p]) | int(b[p + 1]) << 8;
  return int16_t(v < 32768 ? v : v - 65536);
}
int8_t s8(uint8_t v) { return int8_t(v < 128 ? int(v) : int(v) - 256); }
float f32(const Bytes &b, size_t p) {
  auto bits = u32(b, p);
  float v;
  std::memcpy(&v, &bits, 4);
  if (!std::isfinite(v))
    throw std::runtime_error("Nonfinite room record value");
  return v;
}
Vec3 vector(const Bytes &b, size_t p) {
  return {f32(b, p), f32(b, p + 4), f32(b, p + 8)};
}
} // namespace
RoomRecords decode_room_records(const Bytes &data) {
  room_info(data);
  size_t rom = u32(data, 16);
  RoomRecords result;
  auto table = [&](size_t index, size_t stride) {
    size_t count = u32(data, rom + 0x80 + index * 4);
    size_t p = u32(data, rom + index * 4);
    range(data, p, count * stride);
    return std::pair<size_t, size_t>{p, count};
  };
  auto [p, count] = table(9, 16);
  result.positions.reserve(count);
  for (size_t i = 0; i < count; ++i, p += 16)
    result.positions.push_back({vector(data, p), u32(data, p + 12)});
  auto entities = [&](size_t index) {
    auto [p, count] = table(index, 36);
    std::vector<RoomEntity> records;
    records.reserve(count);
    for (size_t i = 0; i < count; ++i, p += 36) {
      RoomEntity e;
      e.flags = u32(data, p);
      e.id = uint16_t(u32(data, p + 4));
      e.type = uint16_t(u32(data, p + 4) >> 16);
      e.floor = s8(data[p + 8]);
      e.model_version = data[p + 9];
      e.work_number = data[p + 10];
      e.parameter = s8(data[p + 11]);
      e.position = vector(data, p + 12);
      e.rotation_x = s16(data, p + 24);
      e.rotation_z = s16(data, p + 26);
      e.rotation_y = s16(data, p + 28);
      e.angular_speed = s16(data, p + 30);
      for (size_t j = 0; j < 4; ++j)
        e.hide[j] = s8(data[p + 32 + j]);
      records.push_back(e);
    }
    return records;
  };
  result.enemies = entities(2);
  result.objects = entities(3);
  result.items = entities(4);
  return result;
}
PlayerWorldState enter_room(const RoomRecords &records, size_t position,
                            const RoomCollision &collision,
                            const std::vector<RoomCut> &cuts) {
  if (position >= records.positions.size())
    throw std::runtime_error("Missing room entry position");
  const auto &entry = records.positions[position];
  for (float ground : collision.ground)
    if (!std::isfinite(ground))
      throw std::runtime_error("Nonfinite room entry ground");
  if (!std::isfinite(entry.position.x) || !std::isfinite(entry.position.z))
    throw std::runtime_error("Nonfinite room entry position");
  PlayerWorldState state;
  state.player.position = entry.position;
  state.player.yaw = uint16_t(entry.yaw);
  state.floor = player_floor_number(collision, entry.position.y);
  state.player.position.y = collision.ground[size_t(int(state.floor) + 2)];
  if (!std::isfinite(state.player.position.y))
    throw std::runtime_error("Nonfinite room entry ground");
  state.player.visual_anchor = state.player.position;
  state.player.water = player_in_water(collision, state.player.position);
  state.front_anchor = state.player.position;
  state.front_anchor.y += 12.5f;
  state.cut = find_camera_cut(cuts, state.player.position.x,
                              state.player.position.z, state.floor);
  return state;
}
} // namespace ashford
