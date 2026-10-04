#include "light.hpp"
#include "camera.hpp"
#include "ninja_math.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ashford {
namespace {
struct Reader {
  const Bytes &b;
  void span(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Light range exceeds input");
  }
  uint32_t u(size_t p) const {
    span(p, 4);
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
           uint32_t(b[p + 3]) << 24;
  }
  int32_t i(size_t p) const {
    uint32_t v = u(p);
    int32_t s;
    std::memcpy(&s, &v, 4);
    return s;
  }
  float f(size_t p) const {
    auto v = u(p);
    float f;
    std::memcpy(&f, &v, 4);
    if (!std::isfinite(f))
      throw std::runtime_error("Nonfinite authored light value");
    return f;
  }
  Vec3 v(size_t p) const { return {f(p), f(p + 4), f(p + 8)}; }
  std::array<float, 3> rgb(size_t p) const {
    return {f(p), f(p + 4), f(p + 8)};
  }
};
int32_t wrapped_add(int32_t a, uint32_t b) {
  uint32_t v = uint32_t(a) + b;
  int32_t result;
  std::memcpy(&result, &v, 4);
  return result;
}
} // namespace
RoomLights decode_room_lights(const Bytes &data) {
  room_info(data);
  Reader r{data};
  const auto rom = r.u(16);
  r.span(rom, 184);
  RoomLights result;
  for (size_t table = 0; table < 2; ++table) {
    const size_t pointer = r.u(rom + (table ? 60 : 4)),
                 count = r.u(rom + (table ? 180 : 132));
    if (count > 256 || (count && !pointer))
      throw std::runtime_error("Invalid room light table");
    r.span(pointer, count * 224);
    auto &lights = table ? result.events : result.normal;
    for (size_t i = 0; i < count; ++i) {
      const auto p = pointer + i * 224;
      LightWork l;
      l.flags = r.u(p);
      l.type = r.i(p + 4);
      l.angular_speed = r.i(p + 8);
      l.link = r.u(p + 12);
      l.link_index = r.u(p + 16);
      l.link_joint = r.u(p + 20);
      l.source = r.u(p + 24);
      l.position = r.v(p + 28);
      l.local = r.v(p + 40);
      l.vector = r.v(p + 52);
      l.accumulators = r.rgb(p + 64);
      l.color = r.rgb(p + 76);
      l.near_range = r.f(p + 88);
      l.far_range = r.f(p + 92);
      for (size_t a = 0; a < 5; ++a)
        l.angles[a] = r.i(p + 96 + 4 * a);
      for (size_t a = 0; a < 4; ++a)
        l.counters[a] = r.i(p + 120 + 4 * a);
      l.mode = r.i(p + 116);
      l.work_position = r.v(p + 136);
      l.work_vector = r.v(p + 148);
      l.work_accumulators = r.rgb(p + 160);
      l.work_color = r.rgb(p + 172);
      l.work_near = r.f(p + 184);
      l.work_far = r.f(p + 188);
      for (size_t a = 0; a < 5; ++a)
        l.work_angles[a] = r.i(p + 192 + 4 * a);
      lights.push_back(l);
    }
  }
  return result;
}
void initialize_room_lights(RoomLights &l) {
  for (size_t i = 0; i < l.normal.size(); ++i) {
    if (i < 4)
      l.normal[i].flags &= ~2u;
    l.normal[i].mode = 0;
    l.normal[i].counters = {};
  }
}
void select_light_cut(RoomLights &l, const CameraDefinition &camera) {
  if (l.normal.size() > 256)
    throw std::runtime_error("Light hide mask overflow");
  for (size_t i = 4; i < l.normal.size(); ++i) {
    if (camera.light_hidden[i / 32] & (0x80000000u >> (i % 32)))
      l.normal[i].flags &= ~2u;
    else
      l.normal[i].flags |= 2u;
  }
}
NativeLighting drawing_lights(RoomLights &work, const NativeLighting &source,
                              bool easy, bool half) {
  auto result = source;
  if (easy) {
    float intensity = half ? .6f : 1.f;
    if (!work.normal.empty()) {
      auto &l = work.normal[0];
      if (l.flags & 2) {
        if (l.type == 100) {
          if (l.mode == 0) {
            ++l.mode;
            intensity *= 3.f;
          } else if (l.mode == 1)
            l.flags &= ~2u;
        } else if (l.type == 101) {
          intensity = 3.f * ninja_sin_cos(uint16_t(l.counters[0]))[1];
          l.counters[0] =
              wrapped_add(l.counters[0], uint32_t(l.angular_speed) * 256u);
          if (l.counters[0] > 16383)
            l.flags &= ~2u;
        }
      }
    }
    result.single_vector = {0, -.3f, -.7f};
    result.single_color = {1, 1, 1};
    result.single_intensity = intensity;
    result.single_ambient = .2f;
  } else {
    const float factor = half ? .5f : 1.f;
    for (size_t c = 0; c < 3; ++c) {
      result.direction.color[c] = factor * source.direction.color[c];
      result.single_color[c] = factor * source.single_color[c];
      for (size_t i = 0; i < source.point_count; ++i)
        result.points.at(i).color[c] = factor * source.points.at(i).color[c];
    }
  }
  return result;
}
} // namespace ashford
