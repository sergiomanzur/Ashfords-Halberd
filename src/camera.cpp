#include "camera.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ashford {
namespace {
struct Read {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Camera range exceeds input");
  }
  uint16_t u16(size_t p) const {
    range(p, 2);
    return uint16_t(b[p]) | uint16_t(b[p + 1]) << 8;
  }
  uint32_t u32(size_t p) const {
    return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16;
  }
  float f(size_t p) const {
    auto v = u32(p);
    float x;
    std::memcpy(&x, &v, 4);
    if (!std::isfinite(x))
      throw std::runtime_error("Non-finite camera field");
    return x;
  }
  Vec3 vec(size_t p) const { return {f(p), f(p + 4), f(p + 8)}; }
};
constexpr double radians = 6.283185307179586 / 65536.;
} // namespace
std::vector<RoomCut> decode_room_cuts(const Bytes &data) {
  room_info(data);
  Read r{data};
  auto rom = r.u32(16), p = r.u32(rom), count = r.u32(rom + 128);
  if (count > 4096)
    throw std::runtime_error("Camera cut limit exceeded");
  r.range(p, size_t(count) * 680);
  std::vector<RoomCut> cuts;
  for (size_t i = 0; i < count; ++i) {
    size_t c = p + i * 680;
    RoomCut cut;
    cut.origin = r.vec(c + 8);
    cut.size = r.vec(c + 20);
    for (size_t v = 0; v < 2; ++v) {
      auto start = c + 32 + v * 196;
      auto &d = cut.variants[v];
      d.flags = r.u16(start);
      d.base.position = r.vec(start + 4);
      d.base.distance = r.f(start + 16);
      d.tracking = r.vec(start + 20);
      for (size_t j = 0; j < 4; ++j) {
        d.heights[j] = r.f(start + 32 + j * 4);
        d.rolls[j] = int16_t(r.u16(start + 72 + j * 2));
      }
      for (size_t j = 0; j < 3; ++j)
        d.base.angles[j] = r.u32(start + 52 + j * 4);
      d.base.perspective = r.u32(start + 84);
      for (size_t j = 0; j < 16; ++j)
        d.base.hidden[j] = r.u32(start + 88 + j * 4);
    }
    cuts.push_back(cut);
  }
  return cuts;
}
Camera resolve_camera(const RoomCut &cut, size_t variant) {
  if (variant >= 2)
    throw std::runtime_error("Invalid camera variant");
  const auto &d = cut.variants[variant];
  auto camera = d.base;
  if (d.flags & 32) {
    uint32_t sum = 0;
    for (unsigned i = 0; i < 4; ++i)
      sum += uint32_t(182.04445f * ((camera.perspective >> (8 * i)) & 255));
    camera.perspective = sum / 4;
  }
  camera.position.y +=
      (d.heights[0] + d.heights[1] + d.heights[2] + d.heights[3]) * .25f;
  camera.angles[2] =
      (camera.angles[2] +
       int((d.rolls[0] + d.rolls[1] + d.rolls[2] + d.rolls[3]) * .25f)) &
      65535;
  if (d.flags & 4) {
    Vec3 target{cut.origin.x + cut.size.x * .5f, cut.origin.y + d.tracking.y,
                cut.origin.z + cut.size.z * .5f};
    double x = double(target.x) - camera.position.x,
           y = double(target.y) - camera.position.y,
           z = double(target.z) - camera.position.z;
    camera.angles[0] =
        uint32_t(int(-std::atan2(y, std::hypot(x, z)) / radians)) & 65535;
    camera.angles[1] =
        uint32_t(int(-std::atan2(x, z) / radians) + 32768) & 65535;
  }
  if (camera.perspective < 2 || camera.perspective >= 32768)
    throw std::runtime_error("Unavailable authored camera perspective");
  return camera;
}
Vec3 camera_point(const Camera &camera, Vec3 point) {
  // camera.c::bhControlCamera: T(-ln) Rz Rx Ry T(-position), followed
  // by njSetView's negative-Z depth convention. Angles use a 16-bit turn.
  double x = double(point.x) - camera.position.x,
         y = double(point.y) - camera.position.y,
         z = double(point.z) - camera.position.z;
  auto rotate = [&](int a, int b, uint32_t angle) {
    double c = std::cos((angle & 65535) * radians),
           s = std::sin((angle & 65535) * radians);
    double *values[] = {&x, &y, &z};
    double u = *values[a], v = *values[b];
    *values[a] = c * u - s * v;
    *values[b] = s * u + c * v;
  };
  rotate(2, 0, camera.angles[1]);
  rotate(1, 2, camera.angles[0]);
  rotate(0, 1, camera.angles[2]);
  Vec3 result{float(x), float(y), float(camera.distance - z)};
  if (!std::isfinite(result.x) || !std::isfinite(result.y) ||
      !std::isfinite(result.z))
    throw std::runtime_error("Camera transform overflow");
  return result;
}
} // namespace ashford
