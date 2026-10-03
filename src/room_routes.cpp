#include "room.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp rutchk.c and ROM_WORK disk fields.
namespace ashford {
namespace {
void range(const Bytes &data, size_t offset, size_t count) {
  if (offset > data.size() || count > data.size() - offset)
    throw std::runtime_error("Room route extent outside asset");
}
uint32_t word(const Bytes &data, size_t offset) {
  range(data, offset, 4);
  return uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8 |
         uint32_t(data[offset + 2]) << 16 | uint32_t(data[offset + 3]) << 24;
}
void finite(Vec3 position) {
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z))
    throw std::runtime_error("Nonfinite room route value");
}
float scalar(const Bytes &data, size_t offset) {
  const auto bits = word(data, offset);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite decoded room route value");
  return value;
}
Vec3 end(const CollisionRecord &record) {
  finite(record.position);
  finite(record.size);
  Vec3 out{record.position.x + record.size.x, record.position.y + record.size.y,
           record.position.z + record.size.z};
  finite(out);
  return out;
}
} // namespace
RoomRoutes decode_room_routes(const Bytes &data) {
  room_info(data);
  const size_t rom = word(data, 16), count = word(data, rom + 0xa8);
  if (count > 255)
    throw std::runtime_error("Room route count exceeds original byte IDs");
  RoomRoutes routes;
  if (!count)
    return routes;
  size_t region = word(data, rom + 0x28);
  const size_t matrix = word(data, rom + 0x2c);
  if (!region || !matrix)
    throw std::runtime_error("Missing room route regions or next-hop table");
  range(data, region, count * 36);
  range(data, matrix, count * count);
  routes.regions.reserve(count);
  for (size_t i = 0; i < count; ++i, region += 36) {
    CollisionRecord record;
    record.flags = data[region];
    record.type = data[region + 1];
    record.id = data[region + 2];
    record.floor = int8_t(data[region + 3]);
    record.attribute = word(data, region + 4);
    record.position = {scalar(data, region + 8), scalar(data, region + 12),
                       scalar(data, region + 16)};
    record.size = {scalar(data, region + 20), scalar(data, region + 24),
                   scalar(data, region + 28)};
    for (size_t parameter = 0; parameter < 4; ++parameter)
      record.parameters[parameter] = data[region + 32 + parameter];
    end(record);
    routes.regions.push_back(record);
  }
  // Diagonal bytes (often0xff) are unused by source and remain intact.
  routes.next_hops.assign(data.begin() + matrix,
                          data.begin() + matrix + count * count);
  return routes;
}
uint8_t room_route_id(const RoomRoutes &routes, Vec3 position) {
  if (routes.regions.size() > 255)
    throw std::runtime_error("Room route count exceeds original byte IDs");
  finite(position);
  for (size_t i = 0; i < routes.regions.size(); ++i) {
    const auto &record = routes.regions[i];
    const auto maximum = end(record);
    if (record.position.x <= position.x && maximum.x >= position.x &&
        record.position.y <= position.y && maximum.y >= position.y &&
        record.position.z <= position.z && maximum.z >= position.z)
      return uint8_t(i);
  }
  return 0xff;
}
uint8_t room_route(const RoomRoutes &routes, Vec3 source, Vec3 destination,
                   Vec3 &output) {
  const auto from = room_route_id(routes, source);
  if (from == 0xff)
    return 0xff;
  const auto to = room_route_id(routes, destination);
  if (to == 0xff)
    return 0xff;
  if (from == to) {
    output = destination;
    return 0xff;
  }
  const auto count = routes.regions.size();
  if (routes.next_hops.size() != count * count)
    throw std::runtime_error("Invalid room next-hop matrix extent");
  const auto hop = routes.next_hops[size_t(to) * count + from];
  if (hop >= count)
    throw std::runtime_error("Selected room route hop outside regions");
  const auto &record = routes.regions[hop];
  Vec3 center{record.position.x + .5f * record.size.x,
              record.position.y + .5f * record.size.y,
              record.position.z + .5f * record.size.z};
  finite(center);
  output = center;
  return hop;
}
} // namespace ashford
