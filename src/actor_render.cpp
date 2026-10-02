#include "actor.hpp"
// Adapted from MIT recvx-decomp MdlPut.c::bhCalcTree and
// njplus.c::npSkinConvPreparation/npSkinConvSub/npInitCalcSkin/npCalcSkin.
#include <cmath>
#include <cstring>
#include <functional>
#include <stdexcept>
namespace ashford {
namespace {
struct Reader {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Actor extent exceeds input");
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
      throw std::runtime_error("Non-finite actor value");
    return f;
  }
  Vec3 vec(size_t p) const { return {f32(p), f32(p + 4), f32(p + 8)}; }
};
using Matrix = ModelMatrix;
Matrix identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }
Matrix multiply(const Matrix &a, const Matrix &b) {
  Matrix c{};
  for (size_t i = 0; i < 4; ++i)
    for (size_t j = 0; j < 4; ++j)
      for (size_t k = 0; k < 4; ++k)
        c[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
  return c;
}
Matrix translate(Vec3 v) {
  auto m = identity();
  m[3] = v.x;
  m[7] = v.y;
  m[11] = v.z;
  return m;
}
Matrix rotate(int axis, int angle) {
  auto m = identity();
  double a = double(angle & 65535) * 6.283185307179586 / 65536;
  double c = std::cos(a), s = std::sin(a);
  int i = (axis + 1) % 3, j = (axis + 2) % 3;
  m[i * 4 + i] = c;
  m[j * 4 + j] = c;
  m[i * 4 + j] = -s;
  m[j * 4 + i] = s;
  return m;
}
Vec3 point(const Matrix &m, Vec3 p) {
  Vec3 v{float(m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3]),
         float(m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7]),
         float(m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11])};
  if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
    throw std::runtime_error("Actor transform overflow");
  return v;
}
void write(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
} // namespace
Scene pose_actor(const ActorModel &actor, const std::vector<JointPose> &pose,
                 Vec3 position, uint16_t yaw) {
  if (actor.model.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Actor model exceeds limit");
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z))
    throw std::runtime_error("Non-finite actor position");
  Reader r{actor.model};
  r.range(0, 16);
  if (actor.model[0] != 'M' || actor.model[1] != 'D' || actor.model[2] != 'L' ||
      !(actor.model[3] & 128))
    throw std::runtime_error("Unsupported actor model");
  size_t base = r.u16(4), count = r.u16(6);
  if (base < 16 || count > 4096 || pose.size() > count)
    throw std::runtime_error("Invalid actor skeleton");
  if (!count)
    return {};
  size_t table = base + size_t(r.u32(12));
  r.range(table, count * 52);
  std::vector<Matrix> world(count), unrotated(count);
  std::vector<Vec3> bind(count);
  std::vector<bool> visited(count);
  auto index = [&](uint32_t off) {
    size_t p = base + size_t(off);
    if (p < table || (p - table) % 52 || (p - table) / 52 >= count)
      throw std::runtime_error("Invalid actor joint link");
    return (p - table) / 52;
  };
  auto placement = multiply(translate(position), rotate(1, yaw));
  std::function<void(size_t, const Matrix &, Vec3, size_t)> visit =
      [&](size_t i, const Matrix &parent, Vec3 origin, size_t depth) {
        if (depth > 256 || visited[i])
          throw std::runtime_error("Cyclic or shared actor skeleton");
        visited[i] = true;
        size_t o = table + i * 52;
        auto p = r.vec(o + 8);
        bind[i] = {origin.x + p.x, origin.y + p.y, origin.z + p.z};
        std::array<uint16_t, 3> angles{uint16_t(r.u32(o + 20)),
                                       uint16_t(r.u32(o + 24)),
                                       uint16_t(r.u32(o + 28))};
        if (i < pose.size()) {
          if (pose[i].position)
            p = *pose[i].position;
          if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            throw std::runtime_error("Non-finite joint position");
          angles = pose[i].angles.value_or(std::array<uint16_t, 3>{});
        }
        auto local = translate(p);
        for (int axis : {2, 1, 0})
          local = multiply(local, rotate(axis, angles[axis]));
        world[i] = multiply(parent, local);
        unrotated[i] = world[i];
        for (int axis : {0, 1, 2})
          unrotated[i] =
              multiply(unrotated[i], rotate(axis, -int(angles[axis])));
        auto child = r.u32(o + 44), sibling = r.u32(o + 48);
        if (child != 0xffffffff)
          visit(index(child), world[i], bind[i], depth + 1);
        if (sibling != 0xffffffff)
          visit(index(sibling), parent, origin, depth + 1);
      };
  visit(0, placement, {}, 0);
  for (bool found : visited)
    if (!found)
      throw std::runtime_error("Unreachable actor joint");
  Bytes posed = actor.model;
  auto drawing_matrices = world;
  if (!actor.skin.empty()) {
    auto skin = decode_skin(actor.skin, count);
    struct Extent {
      size_t start, end;
      bool skinned;
    };
    std::vector<Extent> extents;
    // The reference redirects each skin owner to a separate output buffer.
    // This path poses a private byte copy, so reject shared writes rather than
    // silently deforming another owner or a rigid mesh using the same input.
    for (size_t i = 0; i < count; ++i) {
      auto model = r.u32(table + i * 52 + 4);
      if (model == 0xffffffff)
        continue;
      size_t descriptor = base + size_t(model);
      r.range(descriptor, 24);
      auto offset = r.u32(descriptor);
      if (offset == 0xffffffff)
        continue;
      size_t vertex = base + size_t(offset);
      r.range(vertex, 64);
      if (actor.model[vertex] != 51)
        throw std::runtime_error("Unsupported actor vertex chunk");
      size_t end = vertex + 64 + size_t(r.u16(vertex + 6)) * 32;
      r.range(vertex, end - vertex);
      bool skinned = bool(skin[i].flags & 2);
      for (const auto &prior : extents)
        if ((skinned || prior.skinned) && vertex < prior.end &&
            prior.start < end)
          throw std::runtime_error(
              "Overlapping skin vertex buffers require separate outputs");
      extents.push_back({vertex, end, skinned});
    }
    for (size_t i = 0; i < count; ++i)
      if (skin[i].flags & 2) {
        auto model = r.u32(table + i * 52 + 4);
        if (model == 0xffffffff)
          throw std::runtime_error("Skin owner has no model");
        size_t vertex = base + size_t(r.u32(base + size_t(model)));
        r.range(vertex, 64);
        size_t n = r.u16(vertex + 6);
        if (actor.model[vertex] != 51 || n != skin[i].vertices.size())
          throw std::runtime_error("Skin vertex count mismatch");
        r.range(vertex + 64, n * 32);
        for (size_t v = 0; v < n; ++v) {
          const auto &weight = skin[i].vertices[v];
          if (!(skin[weight.joint].flags & 1) ||
              skin[weight.joint].group != skin[i].group)
            throw std::runtime_error("Skin joint belongs to a different group");
          auto local = r.vec(vertex + 64 + v * 32);
          local = {local.x + bind[i].x - bind[weight.joint].x,
                   local.y + bind[i].y - bind[weight.joint].y,
                   local.z + bind[i].z - bind[weight.joint].z};
          auto a = point(unrotated[weight.joint], local),
               b = point(world[weight.joint], local);
          Vec3 out{a.x + weight.rotation_weight * (b.x - a.x),
                   a.y + weight.rotation_weight * (b.y - a.y),
                   a.z + weight.rotation_weight * (b.z - a.z)};
          for (size_t axis = 0; axis < 3; ++axis)
            write(posed, vertex + 64 + v * 32 + axis * 4,
                  axis == 0   ? out.x
                  : axis == 1 ? out.y
                              : out.z);
        }
        // npCalcSkin outputs actor-space vertices and bypasses the owner
        // matrix.
        drawing_matrices[i] = identity();
      }
  }
  Scene result;
  result.meshes = decode_model(posed, 0, false, &drawing_matrices);
  if (!actor.textures.empty())
    result.textures = decode_texture_pack(actor.textures, 0);
  for (auto &mesh : result.meshes) {
    mesh.object += 512;
    for (const auto &triangle : mesh.triangles)
      if (!actor.textures.empty() && triangle.texture != 0xffff &&
          triangle.texture >= result.textures.size())
        throw std::runtime_error("Actor texture outside pack");
  }
  return result;
}
} // namespace ashford
