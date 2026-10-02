// Adapted from MIT AshfordFamily/recvx-decomp hitchk.c::bhCheckWall/
// bhCheckWallEx/bhCheckBox, pwksub.c::bhCheckFloorNum and
// ps2_NaColi.c::njDistanceP2L. Disk records are decoded into native values.
#include "collision.hpp"
#include "collision_shapes.hpp"
#include "player.hpp"
#include <cmath>
#include <cstring>
#include <optional>
#include <stdexcept>
namespace ashford {
namespace {
void range(const Bytes &b, size_t p, size_t n) {
  if (p > b.size() || n > b.size() - p)
    throw std::runtime_error("Collision record outside room");
}
uint32_t u32(const Bytes &b, size_t p) {
  range(b, p, 4);
  return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
         uint32_t(b[p + 3]) << 24;
}
float f32(const Bytes &b, size_t p) {
  auto bits = u32(b, p);
  float v;
  std::memcpy(&v, &bits, 4);
  if (!std::isfinite(v))
    throw std::runtime_error("Nonfinite collision value");
  return v;
}
float truncate_height(float y) {
  float scaled = 100.f * (.001f + y);
  if (!std::isfinite(scaled) || scaled >= 2147483648.f ||
      scaled < -2147483648.f)
    throw std::runtime_error("Collision height outside integer range");
  return int32_t(scaled) / 100.f;
}
} // namespace
RoomCollision decode_room_collision(const Bytes &data) {
  room_info(data);
  size_t rom = u32(data, 16);
  RoomCollision room;
  room.height = f32(data, rom + 0x118);
  for (size_t i = 0; i < 32; ++i)
    room.ground[i] = f32(data, rom + 0x120 + i * 4);
  auto records = [&](size_t index) {
    size_t count = u32(data, rom + 0x80 + index * 4),
           p = u32(data, rom + index * 4);
    range(data, p, count * 36);
    std::vector<CollisionRecord> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i, p += 36) {
      CollisionRecord r;
      r.flags = data[p];
      r.type = data[p + 1];
      r.id = data[p + 2];
      r.floor = data[p + 3] < 128 ? int8_t(data[p + 3])
                                  : int8_t(int(data[p + 3]) - 256);
      r.attribute = u32(data, p + 4);
      r.position = {f32(data, p + 8), f32(data, p + 12), f32(data, p + 16)};
      r.size = {f32(data, p + 20), f32(data, p + 24), f32(data, p + 28)};
      for (size_t j = 0; j < 4; ++j)
        r.parameters[j] = data[p + 32 + j];
      result.push_back(r);
    }
    return result;
  };
  room.walls = records(6);
  room.miscellaneous = records(7);
  room.floors = records(8);
  return room;
}
int8_t player_floor_number(const RoomCollision &room, float height) {
  if (!std::isfinite(height))
    throw std::runtime_error("Nonfinite player floor height");
  int floor = 2;
  for (int i = 0; i < 31; ++i)
    if ((room.ground[i] != 0 && height >= room.ground[i]) ||
        (i == 2 && height >= room.ground[i]))
      floor = i;
  return int8_t(floor - 2);
}
static CollisionResult collide_walls(const RoomCollision &room,
                                     const CollisionBody &body,
                                     std::optional<Vec3> previous) {
  if (!std::isfinite(body.position.x) || !std::isfinite(body.position.z) ||
      !std::isfinite(body.radius) || !std::isfinite(body.height) ||
      body.radius <= 0 || body.height < 0 || !std::isfinite(room.height))
    throw std::runtime_error("Invalid player collision body");
  CollisionResult result{body.position};
  result.floor = body.floor;
  auto &p = result.position;
  if (!std::isfinite(p.y) || (previous && (!std::isfinite(previous->x) ||
                                           !std::isfinite(previous->y) ||
                                           !std::isfinite(previous->z))))
    throw std::runtime_error("Nonfinite collision position");
  if (!previous)
    p.y = truncate_height(p.y);
  for (const auto &r : room.walls) {
    if (!(r.flags & 1))
      continue;
    if (previous && r.type == 6)
      continue;
    if (r.type > 7)
      throw std::runtime_error("Unsupported active wall collision shape");
    auto attr = r.attribute;
    if (r.type < 6 && (((r.type & 1) && (body.flags & 0x400)) ||
                       ((attr & 1) && r.floor != result.floor) ||
                       ((attr & 2) && (body.flags & 0x4000)) ||
                       ((attr & 4) && (body.state_flags & 0x40000000)) ||
                       ((r.type == 2 || r.type == 3) && (attr & 0x40) &&
                        (body.state_flags & 0x4000))))
      continue;
    if (r.type == 7 && (attr & 1) && r.floor != result.floor)
      continue;
    float x = r.position.x, z = r.position.z,
          y = previous ? r.position.y : truncate_height(r.position.y);
    float w = r.size.x, d = r.size.z, h = r.size.y ? r.size.y : room.height;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(w) || !std::isfinite(d) || !std::isfinite(h))
      throw std::runtime_error(
          "Invalid wall collision geometry: type=" + std::to_string(r.type) +
          " width=" + std::to_string(w) + " height=" + std::to_string(h) +
          " depth=" + std::to_string(d));
    if (r.type != 7 && (p.y + body.height < y || p.y > y + h))
      continue;
    auto project = [&](float corner_x, float corner_z, float radius) {
      float dx = corner_x - p.x, dz = corner_z - p.z;
      int angle = int(10430.381f * std::atan2(dx, dz));
      Vec3 direction = player_add_speed({}, uint16_t(angle), -1);
      p.x = corner_x - direction.x * radius;
      p.z = corner_z - direction.z * radius;
    };
    auto box = [&](float bx, float bz, float bw, float bd,
                   unsigned disabled_corners, bool rounded) {
      float min_x = bx - body.radius, min_z = bz - body.radius;
      float width = bw + 2 * body.radius, depth = bd + 2 * body.radius;
      if (p.x - min_x < 0 || p.x - min_x >= width || p.z - min_z < 0 ||
          p.z - min_z >= depth)
        return false;
      bool use_previous = previous && !rounded;
      bool left = (use_previous ? previous->x : p.x) < bx + .5f * bw,
           top = (use_previous ? previous->z : p.z) < bz + .5f * bd;
      float edge_x = left ? min_x : min_x + width,
            edge_z = top ? min_z : min_z + depth;
      float gap_x = std::fabs(p.x - edge_x), gap_z = std::fabs(p.z - edge_z);
      unsigned corner = left ? (top ? 1 : 2) : (top ? 4 : 8);
      if (rounded && gap_x < body.radius && gap_z < body.radius &&
          !(disabled_corners & corner)) {
        float corner_x = left ? bx : bx + bw, corner_z = top ? bz : bz + bd;
        float dx = corner_x - p.x, dz = corner_z - p.z;
        if (std::sqrt(dx * dx + dz * dz) <= body.radius) {
          project(corner_x, corner_z, body.radius);
          return true;
        }
        return false;
      }
      if (gap_x > gap_z)
        p.z = edge_z;
      else
        p.x = edge_x;
      return true;
    };
    auto line_project = [&](Vec3 origin, Vec3 vector) {
      float length = std::sqrt(vector.x * vector.x + vector.y * vector.y +
                               vector.z * vector.z);
      if (!std::isfinite(length) || length == 0)
        throw std::runtime_error("Degenerate wall projection line");
      float inverse = 1.f / length;
      vector.x *= inverse;
      vector.y *= inverse;
      vector.z *= inverse;
      float dx = p.x - origin.x, dy = p.y - origin.y, dz = p.z - origin.z;
      float distance = vector.x * dx + vector.y * dy + vector.z * dz;
      return Vec3{origin.x + vector.x * distance,
                  origin.y + vector.y * distance,
                  origin.z + vector.z * distance};
    };
    if (r.type == 6) {
      if (!(body.flags & 0x100) || p.x - x < 0 || p.x - x >= w || p.z - z < 0 ||
          p.z - z >= d)
        continue;
      if (r.id > 3)
        throw std::runtime_error("Invalid slope direction");
      ++result.hits;
      Vec3 origin, vector;
      switch (r.id) {
      case 0:
        origin = {x, y, p.z};
        vector = {w, h, 0};
        break;
      case 1:
        origin = {p.x, y, z + d};
        vector = {0, h, -d};
        break;
      case 2:
        origin = {p.x, y, z};
        vector = {0, h, d};
        break;
      default:
        origin = {x + w, y, p.z};
        vector = {-w, h, 0};
        break;
      }
      auto point = line_project(origin, vector);
      p.y = point.y;
      for (int j = 0; j < 31; ++j)
        if ((room.ground[j] != 0 && p.y >= room.ground[j]) ||
            (j == 2 && p.y >= room.ground[j]))
          result.floor = int8_t(j - 2);
      int ground_index = int(result.floor) + 2;
      if (ground_index < 0 || ground_index >= 32)
        throw std::runtime_error("Slope floor outside ground table");
      float ground = room.ground[size_t(ground_index)];
      if (!std::isfinite(ground))
        throw std::runtime_error("Nonfinite collision ground height");
      if (result.floor != r.floor) {
        if (p.y < ground) {
          p.x = point.x;
          p.z = point.z;
        } else
          p.y = ground;
      } else {
        p.x = point.x;
        p.z = point.z;
        if (p.y < ground)
          p.y = ground;
      }
    } else if (r.type == 7) {
      bool grounded = body.flags & 0x100;
      bool crosses = grounded ? (p.y < y + h && p.y + body.height > y + h)
                              : (p.y + body.height >= y + h && p.y < y);
      if (crosses && box(x, z, w, d, 0, false))
        ++result.hits;
    } else if (r.type == 4 || r.type == 5) {
      if (r.id > 3 || w == 0 || d == 0)
        throw std::runtime_error("Invalid triangle wall geometry");
      if (triangle_wall_inside(r, p, body.radius, body.height, room.height)) {
        ++result.hits;
        float tx = x + ((r.id < 2) ? body.radius : -body.radius);
        float tz = z + ((r.id == 0 || r.id == 2) ? body.radius : -body.radius);
        auto point = line_project({tx, 0, tz + d}, {w, 0, -d});
        p.x = point.x;
        p.z = point.z;
      } else {
        switch (r.id) {
        case 0:
          result.hits += box(x, z, 0, d, 12, true);
          result.hits += box(x, z, w, 0, 10, true);
          break;
        case 1:
          result.hits += box(x, z + d, 0, -d, 12, true);
          result.hits += box(x, z, w, 0, 5, true);
          break;
        case 2:
          result.hits += box(x, z, 0, d, 3, true);
          result.hits += box(x + w, z, -w, 0, 10, true);
          break;
        case 3:
          result.hits += box(x, z + d, 0, -d, 3, true);
          result.hits += box(x + w, z, -w, 0, 5, true);
          break;
        }
      }
    } else if (r.type >= 2) {
      float dx = x - p.x, dz = z - p.z;
      float radius = body.radius + w;
      if (std::sqrt(dx * dx + dz * dz) < radius) {
        if ((attr & 0x40) && (body.state_flags & 0x40000000))
          throw std::runtime_error(
              "Special wall damage response requires player state");
        ++result.hits;
        project(x, z, radius);
      }
    } else {
      float min_x = x - body.radius, min_z = z - body.radius;
      float width = w + 2 * body.radius, depth = d + 2 * body.radius;
      if (p.x - min_x < 0 || p.x - min_x >= width || p.z - min_z < 0 ||
          p.z - min_z >= depth)
        continue;
      ++result.hits;
      bool left = (previous ? previous->x : p.x) < x + .5f * w,
           top = (previous ? previous->z : p.z) < z + .5f * d;
      float edge_x = left ? min_x : min_x + width,
            edge_z = top ? min_z : min_z + depth;
      float gap_x = std::fabs(p.x - edge_x), gap_z = std::fabs(p.z - edge_z);
      if (gap_x < body.radius && gap_z < body.radius) {
        float corner_x = left ? x : x + w, corner_z = top ? z : z + d;
        float dx = corner_x - p.x, dz = corner_z - p.z;
        if (std::sqrt(dx * dx + dz * dz) <= body.radius)
          project(corner_x, corner_z, body.radius);
        result.reset_push_count = true;
      } else {
        if (previous) {
          gap_x = std::fabs(previous->x - edge_x);
          gap_z = std::fabs(previous->z - edge_z);
        }
        if (gap_x > gap_z)
          p.z = edge_z;
        else
          p.x = edge_x;
      }
    }
  }
  if (result.hits == 0 && (body.flags & 0x100)) {
    int index = int(result.floor) + 2;
    if (index < 0 || index >= 32)
      throw std::runtime_error("Player floor outside ground table");
    p.y = room.ground[size_t(index)];
  }
  if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
    throw std::runtime_error("Collision response overflow");
  return result;
}
CollisionResult collide_player_walls(const RoomCollision &room,
                                     const CollisionBody &body) {
  return collide_walls(room, body, std::nullopt);
}
CollisionResult collide_player_walls_ex(const RoomCollision &room,
                                        const CollisionBody &body,
                                        Vec3 previous) {
  return collide_walls(room, body, previous);
}
} // namespace ashford
