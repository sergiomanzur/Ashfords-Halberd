#include "actor.hpp"
#include "ninja_matrix.hpp"
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
void validate_matrix(const Matrix &m) {
  for (double value : m)
    if (!std::isfinite(value))
      throw std::runtime_error("Non-finite actor matrix");
  if (m[12] != 0 || m[13] != 0 || m[14] != 0 || m[15] != 1)
    throw std::runtime_error("Actor matrix is not affine");
}
Matrix placement_matrix(Vec3 position, std::array<int32_t, 3> angles) {
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z))
    throw std::runtime_error("Non-finite actor position");
  auto m = translate(position);
  for (int axis : {2, 1, 0})
    m = multiply(m, rotate(axis, angles[axis]));
  return m;
}
struct SkeletonTransforms {
  size_t base = 0, count = 0, table = 0;
  std::vector<Matrix> world, unrotated;
  std::vector<Vec3> bind;
};
SkeletonTransforms calculate_skeleton(const ActorModel &actor,
                                      const std::vector<JointPose> &pose,
                                      const Matrix &placement) {
  if (actor.model.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Actor model exceeds limit");
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
  SkeletonTransforms result{base,
                            count,
                            table,
                            std::vector<Matrix>(count),
                            std::vector<Matrix>(count),
                            std::vector<Vec3>(count)};
  auto &world = result.world, &unrotated = result.unrotated;
  auto &bind = result.bind;
  std::vector<bool> visited(count);
  auto index = [&](uint32_t off) {
    size_t p = base + size_t(off);
    if (p < table || (p - table) % 52 || (p - table) / 52 >= count)
      throw std::runtime_error("Invalid actor joint link");
    return (p - table) / 52;
  };
  std::function<void(size_t, const Matrix &, Vec3, size_t)> visit =
      [&](size_t i, const Matrix &parent, Vec3 origin, size_t depth) {
        if (depth > 256 || visited[i])
          throw std::runtime_error("Cyclic or shared actor skeleton");
        visited[i] = true;
        size_t o = table + i * 52;
        auto p = r.vec(o + 8);
        bind[i] = {origin.x + p.x, origin.y + p.y, origin.z + p.z};
        std::array<int32_t, 3> angles{int32_t(r.u32(o + 20)),
                                      int32_t(r.u32(o + 24)),
                                      int32_t(r.u32(o + 28))};
        if (i < pose.size()) {
          if (pose[i].position)
            p = *pose[i].position;
          if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            throw std::runtime_error("Non-finite joint position");
          angles = pose[i].angles.value_or(std::array<int32_t, 3>{});
        }
        auto local = translate(p);
        for (int axis : {2, 1, 0})
          local = multiply(local, rotate(axis, angles[axis]));
        world[i] = multiply(parent, local);
        validate_matrix(world[i]);
        unrotated[i] = world[i];
        for (int axis : {0, 1, 2})
          unrotated[i] = multiply(unrotated[i],
                                  rotate(axis, -int(uint16_t(angles[axis]))));
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
  return result;
}
size_t work_count(const ActorModelWork &work, size_t count) {
  if (work.first_joint > count || work.joints.size() != count)
    throw std::runtime_error("Invalid actor model work range");
  const auto selected = work.joint_count.value_or(count - work.first_joint);
  if (selected > count - work.first_joint)
    throw std::runtime_error("Invalid actor model work count");
  return selected;
}
Scene draw_actor(const ActorModel &actor, const std::vector<JointPose> &pose,
                 const Matrix &placement,
                 const ActorModelWork *work = nullptr) {
  if (actor.skin_layout == SkinLayout::FaceModel && !actor.skin_preconverted)
    throw std::runtime_error("Facial skin must be converted before expansion");
  auto skeleton = calculate_skeleton(actor, pose, placement);
  const auto base = skeleton.base, count = skeleton.count,
             table = skeleton.table;
  size_t first = 0, selected = count;
  if (work) {
    selected = work_count(*work, count);
    first = work->first_joint;
    Reader r{actor.model};
    for (size_t i = 0; i < count; ++i) {
      validate_matrix(work->joints[i].matrix);
      skeleton.world[i] = work->joints[i].matrix;
      skeleton.unrotated[i] = skeleton.world[i];
      const auto o = table + i * 52;
      std::array<int32_t, 3> angles{int32_t(r.u32(o + 20)),
                                    int32_t(r.u32(o + 24)),
                                    int32_t(r.u32(o + 28))};
      if (i < pose.size())
        angles = pose[i].angles.value_or(std::array<int32_t, 3>{});
      for (int axis : {0, 1, 2})
        skeleton.unrotated[i] = multiply(
            skeleton.unrotated[i], rotate(axis, -int(uint16_t(angles[axis]))));
    }
    if (!selected)
      return {};
  }
  if (!count)
    return {};
  const auto &world = skeleton.world, &unrotated = skeleton.unrotated;
  const auto &bind = skeleton.bind;
  Reader r{actor.model};
  Bytes posed = actor.model;
  auto drawing_matrices = world;
  if (!actor.skin.empty()) {
    auto skin = decode_skin(actor.skin, count, actor.skin_layout);
    struct Extent {
      size_t start, end;
      bool skinned;
    };
    std::vector<Extent> extents;
    // The reference redirects each skin owner to a separate output buffer.
    // This path poses a private byte copy, so reject shared writes rather than
    // silently deforming another owner or a rigid mesh using the same input.
    for (size_t i = first; i < first + selected; ++i) {
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
    for (size_t i = first; i < first + selected; ++i)
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
          if (!actor.skin_preconverted)
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
  result.meshes =
      work ? decode_model_range(posed, 0, drawing_matrices, first, selected)
           : decode_model(posed, 0, false, &drawing_matrices);
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
} // namespace
ActorModel prepare_actor_skin(const ActorModel &actor) {
  if (actor.skin_layout != SkinLayout::Standard)
    throw std::runtime_error(
        "Ordinary skin preparation requires standard layout");
  if (actor.skin_preconverted)
    return actor;
  const auto skeleton = calculate_skeleton(actor, {}, identity());
  auto out = actor;
  if (!actor.skin.empty()) {
    Reader r{actor.model};
    const auto skin = decode_skin(actor.skin, skeleton.count);
    struct Extent {
      size_t begin, end;
      bool writable;
    };
    std::vector<Extent> extents;
    for (size_t i = 0; i < skeleton.count; ++i) {
      const auto object = skeleton.table + i * 52;
      if (r.u32(object) & 0x20000000)
        throw std::runtime_error(
            "Expanded objects require prepared skin layout");
      const auto model = r.u32(object + 4);
      const bool writable = bool(skin[i].flags & 2);
      if (model == 0xffffffff) {
        if (writable)
          throw std::runtime_error("Skin owner has no model");
        continue;
      }
      const auto descriptor = skeleton.base + size_t(model);
      r.range(descriptor, 24);
      const auto vertex = skeleton.base + size_t(r.u32(descriptor));
      r.range(vertex, 64);
      if (actor.model[vertex] != 51)
        throw std::runtime_error("Unsupported actor vertex chunk");
      const auto count = size_t(r.u16(vertex + 6));
      const auto end = vertex + 64 + count * 32;
      r.range(vertex, end - vertex);
      for (const auto &prior : extents)
        if ((writable || prior.writable) && vertex < prior.end &&
            prior.begin < end)
          throw std::runtime_error(
              "Overlapping skin vertex buffers require separate outputs");
      extents.push_back({vertex, end, writable});
      if (!writable)
        continue;
      if (count != skin[i].vertices.size())
        throw std::runtime_error("Skin vertex count mismatch");
      const auto flags = r.u32(object) | 0x80000000u;
      for (size_t byte = 0; byte < 4; ++byte)
        out.model[object + byte] = uint8_t(flags >> (byte * 8));
      for (size_t v = 0; v < count; ++v) {
        const auto joint = skin[i].vertices[v].joint;
        if (!(skin[joint].flags & 1) || skin[joint].group != skin[i].group)
          throw std::runtime_error("Skin joint belongs to a different group");
        const auto p = r.vec(vertex + 64 + v * 32), a = skeleton.bind[i],
                   b = skeleton.bind[joint];
        write(out.model, vertex + 64 + v * 32, p.x + a.x - b.x);
        write(out.model, vertex + 68 + v * 32, p.y + a.y - b.y);
        write(out.model, vertex + 72 + v * 32, p.z + a.z - b.z);
      }
    }
  }
  out.skin_preconverted = true;
  return out;
}
ActorModelWork calculate_actor_model_work(const ActorModel &actor,
                                          const std::vector<JointPose> &pose,
                                          ActorModelWork work,
                                          const ActorPlacement &placement) {
  // Full immutable hierarchy validation and bind data stay separate from the
  // source's selected linear O_WORK traversal.
  const auto skeleton = calculate_skeleton(actor, pose, identity());
  if (work.joints.empty())
    work.joints.resize(skeleton.count);
  const auto selected = work_count(work, skeleton.count);
  auto base = detail::ninja_identity();
  if (placement.flags & 0x80) {
    if (!placement.parent_joint)
      throw std::runtime_error(
          "Linked actor model work requires parent matrix");
    validate_matrix(*placement.parent_joint);
    base = detail::ninja_load(*placement.parent_joint);
    detail::ninja_translate(base, placement.linked_offset);
    if (placement.flags & 0x1000)
      for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 3; ++column)
          base[row * 4 + column] = row == column ? 1 : 0;
    detail::ninja_rotate(base, placement.angles);
  } else if (!(placement.flags & 0x1000)) {
    detail::ninja_translate(base, placement.position);
    detail::ninja_rotate(base, placement.angles);
  } else
    base = detail::ninja_load(work.base_matrix);
  work.base_matrix = detail::ninja_store(base);
  validate_matrix(work.base_matrix);
  Reader r{actor.model};
  auto current = base;
  // The final leaf restores the caller's matrix, which is unused afterward.
  std::vector<detail::NinjaMatrix> stack{current};
  for (size_t i = work.first_joint; i < work.first_joint + selected; ++i) {
    const size_t o = skeleton.table + i * 52;
    if (r.u32(o + 48) != 0xffffffff)
      stack.push_back(current);
    auto p = r.vec(o + 8);
    std::array<int32_t, 3> angles{
        int32_t(r.u32(o + 20)), int32_t(r.u32(o + 24)), int32_t(r.u32(o + 28))};
    if (i < pose.size()) {
      p = pose[i].position.value_or(p);
      angles = pose[i].angles.value_or(std::array<int32_t, 3>{});
    }
    detail::ninja_translate(current, p);
    detail::ninja_rotate(current, angles);
    if (work.joints[i].flags & 1)
      current = detail::ninja_load(work.joints[i].matrix);
    work.joints[i].matrix = detail::ninja_store(current);
    if (r.u32(o + 44) == 0xffffffff) {
      if (stack.empty())
        throw std::runtime_error(
            "Actor model range exceeds native matrix stack");
      current = stack.back();
      stack.pop_back();
    }
  }
  return work;
}
Scene pose_actor_model_work(const ActorModel &actor,
                            const std::vector<JointPose> &pose,
                            const ActorModelWork &work) {
  return draw_actor(actor, pose, identity(), &work);
}
std::vector<JointPose> actor_bind_pose(const ActorModel &actor) {
  Reader r{actor.model};
  r.range(0, 16);
  if (actor.model[0] != 'M' || actor.model[1] != 'D' || actor.model[2] != 'L' ||
      !(actor.model[3] & 128))
    throw std::runtime_error("Unsupported actor bind model");
  size_t base = r.u16(4), count = r.u16(6);
  if (base < 16 || count > 4096)
    throw std::runtime_error("Invalid actor bind skeleton");
  size_t table = base + size_t(r.u32(12));
  r.range(table, count * 52);
  std::vector<JointPose> pose(count);
  for (size_t i = 0; i < count; ++i) {
    size_t p = table + i * 52;
    pose[i].position = r.vec(p + 8);
    pose[i].angles = std::array<int32_t, 3>{
        int32_t(r.u32(p + 20)), int32_t(r.u32(p + 24)), int32_t(r.u32(p + 28))};
  }
  return pose;
}
Scene pose_actor(const ActorModel &actor, const std::vector<JointPose> &pose,
                 Vec3 position, uint16_t yaw) {
  return pose_actor_rotated(actor, pose, position, {0, int32_t(yaw), 0});
}
Scene pose_actor_rotated(const ActorModel &actor,
                         const std::vector<JointPose> &pose, Vec3 position,
                         std::array<int32_t, 3> angles) {
  return draw_actor(actor, pose, placement_matrix(position, angles));
}
std::vector<ModelMatrix>
actor_joint_matrices(const ActorModel &actor,
                     const std::vector<JointPose> &pose, Vec3 position,
                     std::array<int32_t, 3> angles) {
  return calculate_skeleton(actor, pose, placement_matrix(position, angles))
      .world;
}
Scene pose_actor_linked(const ActorModel &actor,
                        const std::vector<JointPose> &pose,
                        const ModelMatrix &parent_joint, Vec3 offset,
                        std::array<int32_t, 3> angles) {
  validate_matrix(parent_joint);
  return draw_actor(actor, pose,
                    multiply(parent_joint, placement_matrix(offset, angles)));
}

} // namespace ashford
