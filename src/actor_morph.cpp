#include "actor.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
// MIT recvx-decomp njplus.c::npCalcMorphing/npTransform.
namespace ashford {
namespace {
struct Reader {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Morph extent exceeds model");
  }
  uint16_t u16(size_t p) const {
    range(p, 2);
    return uint16_t(b[p]) | uint16_t(b[p + 1]) << 8;
  }
  uint32_t u32(size_t p) const {
    return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16;
  }
  float f32(size_t p) const {
    auto v = u32(p);
    float f;
    std::memcpy(&f, &v, 4);
    if (!std::isfinite(f))
      throw std::runtime_error("Nonfinite morph input");
    return f;
  }
};
void put(Bytes &b, size_t p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(value >> (8 * i));
}
void write(Bytes &b, size_t p, float f) {
  if (!std::isfinite(f))
    throw std::runtime_error("Morph interpolation overflow");
  uint32_t value;
  std::memcpy(&value, &f, 4);
  put(b, p, value);
}
struct Objects {
  size_t base, table, count;
};
Objects objects(const Reader &r) {
  r.range(0, 16);
  if (r.b.size() > 64 * 1024 * 1024 || r.b[0] != 'M' || r.b[1] != 'D' ||
      r.b[2] != 'L' || !(r.b[3] & 128))
    throw std::runtime_error("Invalid morph model");
  Objects out{r.u16(4), 0, r.u16(6)};
  if (out.base < 16 || out.count > 4096)
    throw std::runtime_error("Invalid morph object count");
  out.table = out.base + size_t(r.u32(12));
  r.range(out.table, out.count * 52);
  return out;
}
size_t vertices(const Reader &r, size_t base, size_t object) {
  auto model = r.u32(object + 4);
  if (model == 0xffffffff)
    throw std::runtime_error("Morph target lacks model");
  const auto descriptor = base + size_t(model);
  r.range(descriptor, 24);
  const auto vertex = base + size_t(r.u32(descriptor));
  r.range(vertex, 64);
  if (r.b[vertex] != 51)
    throw std::runtime_error("Unsupported morph vertex chunk");
  return vertex;
}
} // namespace
ActorModel morph_actor_model(const ActorModel &source, const ActorModel &target,
                             const std::vector<JointPose> &pose, float amount,
                             size_t first, std::optional<size_t> selected) {
  Reader a{source.model}, b{target.model};
  const auto src = objects(a);
  if (first > src.count || pose.size() > src.count)
    throw std::runtime_error("Invalid morph source range");
  const auto count = selected.value_or(src.count - first);
  if (count > src.count - first)
    throw std::runtime_error("Invalid morph selected count");
  auto out = source;
  std::optional<Objects> dst;
  for (size_t i = first; i < first + count; ++i) {
    const auto object = src.table + i * 52;
    if (a.u32(object + 4) == 0xffffffff || (a.u32(object) & 8))
      continue;
    if (!std::isfinite(amount))
      throw std::runtime_error("Invalid morph amount");
    if ((!source.skin.empty() && !source.skin_preconverted) ||
        (!target.skin.empty() && !target.skin_preconverted))
      throw std::runtime_error("Morph skin requires bind-converted inputs");
    if (!dst)
      dst = objects(b);
    if (i >= dst->count)
      throw std::runtime_error("Missing morph target object");
    const auto target_object = dst->table + i * 52;
    Vec3 p{a.f32(object + 8), a.f32(object + 12), a.f32(object + 16)};
    if (i < pose.size() && pose[i].position)
      p = *pose[i].position;
    for (size_t axis = 0; axis < 3; ++axis) {
      const float value = axis == 0 ? p.x : axis == 1 ? p.y : p.z;
      write(out.model, object + 8 + axis * 4,
            amount * (.001f * (b.f32(target_object + 8 + axis * 4) - value)) +
                value);
    }
    const auto av = vertices(a, src.base, object),
               bv = vertices(b, dst->base, target_object);
    const auto n = size_t(a.u16(av + 6));
    if (n != b.u16(bv + 6))
      continue;
    const auto length = 64 + n * 32;
    a.range(av, length);
    b.range(bv, length);
    const auto output = out.model.size();
    if (output + length + 2 > 64 * 1024 * 1024)
      throw std::runtime_error("Morphed model exceeds limit");
    out.model.insert(out.model.end(), source.model.begin() + av,
                     source.model.begin() + av + length);
    out.model.push_back(255);
    out.model.push_back(0);
    const float rate = amount * .001f;
    for (size_t v = 0; v < n; ++v) {
      const auto s = av + 64 + v * 32, t = bv + 64 + v * 32,
                 d = output + 64 + v * 32;
      bool changed = false;
      for (size_t axis = 0; axis < 3; ++axis)
        changed |= a.f32(s + axis * 4) != b.f32(t + axis * 4);
      if (!changed)
        continue;
      // VU xyz-only instructions retain target W lanes for changed vertices.
      put(out.model, d + 12, b.u32(t + 12));
      put(out.model, d + 28, b.u32(t + 28));
      for (size_t axis = 0; axis < 3; ++axis) {
        const auto offset = axis * 4;
        const auto value = a.f32(s + offset), normal = a.f32(s + 16 + offset);
        write(out.model, d + offset,
              value + rate * (b.f32(t + offset) - value));
        write(out.model, d + 16 + offset,
              normal + rate * (b.f32(t + 16 + offset) - normal));
      }
    }
    const auto descriptor = src.base + size_t(a.u32(object + 4));
    put(out.model, descriptor, uint32_t(output - src.base));
  }
  return out;
}
} // namespace ashford
