#include "face_model.hpp"
// Adapted from MIT recvx-decomp face_bh.c fmCnkInitContext/_fmCnkSet*
// and njplus.c npSkinConvert. Retail input is immutable; offsets replace
// the original 32-bit pointers in private native-owned model storage.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <stdexcept>
namespace ashford {
namespace {
constexpr uint32_t none = 0xffffffff;
struct Reader {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Facial model extent exceeds input");
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
      throw std::runtime_error("Non-finite facial value");
    return f;
  }
  Vec3 vec(size_t p) const { return {f32(p), f32(p + 4), f32(p + 8)}; }
};
void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
void half(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
void flt(Bytes &b, size_t p, float f) {
  if (!std::isfinite(f))
    throw std::runtime_error("Facial conversion overflow");
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
struct Node {
  std::array<uint8_t, 52> bytes;
  uint32_t model = none, child = none, sibling = none;
};
struct Geometry {
  size_t descriptor, vertices, count, end;
};
Geometry geometry(const Bytes &b, size_t base, uint32_t off) {
  Reader r{b};
  if (off == none)
    throw std::runtime_error("Facial part has no model");
  size_t d = base + size_t(off);
  r.range(d, 24);
  auto vertex = r.u32(d);
  if (vertex == none)
    throw std::runtime_error("Facial part has no vertices");
  size_t v = base + size_t(vertex);
  r.range(v, 64);
  if (b[v] != 51)
    throw std::runtime_error("Unsupported facial vertex chunk");
  size_t n = r.u16(v + 6), end = v + 64 + n * 32;
  r.range(v, end - v);
  return {d, v, n, end};
}
void preconvert(ActorModel &actor, const std::vector<Node> &nodes,
                size_t base) {
  // npSkinConvPreparation uses translations only, before any face wrappers.
  const auto skin = decode_skin(actor.skin, nodes.size());
  const auto source = actor.model;
  Reader r{source};
  std::vector<Vec3> bind(nodes.size());
  std::function<void(size_t, Vec3)> walk = [&](size_t i, Vec3 p) {
    const auto &n = nodes[i];
    Bytes bytes(n.bytes.begin(), n.bytes.end());
    Reader nr{bytes};
    if (nr.u32(0) & 0x20000000)
      throw std::runtime_error(
          "Pre-expanded facial input requires another skin layout");
    auto local = nr.vec(8);
    bind[i] = {p.x + local.x, p.y + local.y, p.z + local.z};
    if (n.child != none)
      walk(n.child, bind[i]);
    if (n.sibling != none)
      walk(n.sibling, p);
  };
  walk(0, {});
  struct Extent {
    size_t start, end;
    bool writable;
  };
  std::vector<Extent> extents;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].model == none) {
      if (skin[i].flags & 2)
        throw std::runtime_error("Skin owner has no model");
      continue;
    }
    auto g = geometry(source, base, nodes[i].model);
    bool writable = bool(skin[i].flags & 2);
    for (const auto &prior : extents)
      if ((writable || prior.writable) && g.vertices < prior.end &&
          prior.start < g.end)
        throw std::runtime_error(
            "Overlapping skin vertex buffers require separate outputs");
    extents.push_back({g.vertices, g.end, writable});
    if (!writable)
      continue;
    if (g.count != skin[i].vertices.size())
      throw std::runtime_error("Skin vertex count mismatch");
    for (size_t v = 0; v < g.count; ++v) {
      auto joint = skin[i].vertices[v].joint;
      if (!(skin[joint].flags & 1) || skin[joint].group != skin[i].group)
        throw std::runtime_error("Skin joint belongs to a different group");
      auto p = r.vec(g.vertices + 64 + v * 32);
      flt(actor.model, g.vertices + 64 + v * 32,
          p.x + bind[i].x - bind[joint].x);
      flt(actor.model, g.vertices + 68 + v * 32,
          p.y + bind[i].y - bind[joint].y);
      flt(actor.model, g.vertices + 72 + v * 32,
          p.z + bind[i].z - bind[joint].z);
    }
  }
}
FaceGeometry private_geometry(ActorModel &actor, size_t base, Node &node) {
  auto g = geometry(actor.model, base, node.model);
  Reader r{actor.model};
  r.range(g.end, 2);
  if (actor.model[g.end] != 255)
    throw std::runtime_error("Unsupported facial vertex stream");
  Bytes descriptor(actor.model.begin() + g.descriptor,
                   actor.model.begin() + g.descriptor + 24);
  Bytes vertices(actor.model.begin() + g.vertices,
                 actor.model.begin() + g.end + 2);
  size_t d = actor.model.size(), v = d + 24;
  put(descriptor, 0, uint32_t(v - base));
  actor.model.insert(actor.model.end(), descriptor.begin(), descriptor.end());
  actor.model.insert(actor.model.end(), vertices.begin(), vertices.end());
  node.model = uint32_t(d - base);
  return {g.descriptor, g.vertices, d, v, g.count};
}
void validate_mask_tables(const Bytes &mask, size_t face_vertices,
                          size_t tongue_vertices) {
  Reader r{mask};
  auto lists = r.u32(0x10), connections = r.u32(0x14), jaws = r.u32(0x18),
       tongues = r.u32(0x1c), shapes = r.u32(0x20);
  for (auto n : {lists, connections, jaws, tongues, shapes})
    if (n > 1000000)
      throw std::runtime_error("Facial table count exceeds limit");
  size_t p = 64, indices = 0;
  r.range(p, size_t(lists) * 4);
  for (size_t i = 0; i < lists; ++i) {
    auto count = mask[p + i * 4 + 3];
    if (count > 127)
      throw std::runtime_error("Invalid facial vertex list count");
    indices += count;
  }
  p += size_t(lists) * 4;
  r.range(p, indices * 4);
  for (size_t i = 0; i < indices; ++i)
    if (r.u32(p + i * 4) >= face_vertices)
      throw std::runtime_error("Facial vertex reference outside model");
  p += indices * 4;
  r.range(p, size_t(connections) * 20);
  // Deformation semantics are separate; enforce byte and vertex bounds here.
  p += size_t(connections) * 20;
  for (auto table : {std::pair<uint32_t, size_t>{jaws, face_vertices},
                     {tongues, tongue_vertices}}) {
    r.range(p, size_t(table.first) * 8);
    for (size_t i = 0; i < table.first; ++i)
      if (r.u32(p + i * 8) >= table.second)
        throw std::runtime_error("Facial weighted vertex outside model");
    p += size_t(table.first) * 8;
  }
  r.range(p, size_t(shapes) * 164);
}
} // namespace
PreparedFaceModel prepare_face_model(const ActorModel &actor,
                                     const Bytes &mask) {
  if (actor.skin_layout != SkinLayout::Standard || actor.skin_preconverted)
    throw std::runtime_error("Facial model is already prepared");
  if (mask.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Facial MASK exceeds limit");
  Reader mr{mask};
  mr.range(0, 64);
  if (std::string(mask.begin(), mask.begin() + 4) != "MASK")
    throw std::runtime_error("Invalid facial MASK signature");
  // Existing traversal validates graph bounds, cycles, shared links and values.
  actor_joint_matrices(actor, {});
  Reader r{actor.model};
  size_t base = r.u16(4), count = r.u16(6), table = base + size_t(r.u32(12));
  if (count < 11 || count > 4092 || actor.skin.empty())
    throw std::runtime_error("Unsupported facial skeleton");
  std::vector<Node> nodes(count);
  nodes.reserve(count + 4);
  auto index = [&](uint32_t off) {
    return off == none ? none : uint32_t((base + size_t(off) - table) / 52);
  };
  for (size_t i = 0; i < count; ++i) {
    size_t p = table + i * 52;
    std::copy_n(actor.model.begin() + p, 52, nodes[i].bytes.begin());
    nodes[i].model = r.u32(p + 4);
    nodes[i].child = index(r.u32(p + 44));
    nodes[i].sibling = index(r.u32(p + 48));
    for (size_t axis = 0; axis < 3; ++axis)
      r.f32(p + 32 + axis * 4);
  }
  PreparedFaceModel out;
  out.actor = actor;
  preconvert(out.actor, nodes, base);
  const auto skin = decode_skin(actor.skin, count);
  for (size_t i = 0; i < count; ++i)
    if (skin[i].flags & 2) {
      Bytes bytes(nodes[i].bytes.begin(), nodes[i].bytes.end());
      put(bytes, 0, Reader{bytes}.u32(0) | 0x80000000);
      std::copy(bytes.begin(), bytes.end(), nodes[i].bytes.begin());
    }
  std::vector<uint32_t> wrapped;
  auto search = [&](uint8_t id) {
    if (id == 255)
      return none;
    if (id >= 128)
      throw std::runtime_error("Invalid signed facial object ID");
    uint32_t result = none;
    size_t ordinal = 0;
    std::function<void(uint32_t)> visit = [&](uint32_t i) {
      if (i == none || result != none)
        return;
      if (nodes[i].model != none && ordinal++ == id) {
        result = i;
        return;
      }
      if (std::find(wrapped.begin(), wrapped.end(), i) == wrapped.end())
        visit(nodes[i].child);
      visit(nodes[i].sibling);
    };
    visit(0);
    if (result == none)
      throw std::runtime_error("Facial object ID outside model");
    return result;
  };
  auto face = search(mask[0x34]);
  if (face == none)
    throw std::runtime_error("Required face object is absent");
  out.face_geometry = private_geometry(out.actor, base, nodes[face]);
  auto wrap = [&](uint32_t parent) {
    if (parent == none || parent == face ||
        std::find(wrapped.begin(), wrapped.end(), parent) != wrapped.end())
      throw std::runtime_error("Missing or duplicate facial part");
    Node clone = nodes[parent];
    clone.child = none;
    clone.sibling = nodes[parent].child;
    Bytes bytes(nodes[parent].bytes.begin(), nodes[parent].bytes.end());
    Reader nr{bytes};
    FacePart part;
    part.parent = parent;
    part.clone = nodes.size();
    part.original_position = nr.vec(8);
    for (size_t axis = 0; axis < 3; ++axis)
      part.original_angles[axis] = int32_t(nr.u32(20 + axis * 4));
    Bytes copy(clone.bytes.begin(), clone.bytes.end());
    for (size_t axis = 0; axis < 3; ++axis) {
      flt(copy, 8 + axis * 4, 0);
      flt(bytes, 32 + axis * 4, 1);
      put(bytes, 20 + axis * 4, 0);
    }
    put(bytes, 0, (nr.u32(0) | 0xa0000008) & ~0x12u);
    std::copy(copy.begin(), copy.end(), clone.bytes.begin());
    std::copy(bytes.begin(), bytes.end(), nodes[parent].bytes.begin());
    nodes[parent].child = uint32_t(part.clone);
    nodes.push_back(clone);
    wrapped.push_back(parent);
    return part;
  };
  out.tooth = wrap(search(mask[0x36]));
  auto tongue = search(mask[0x35]);
  if (tongue == none || tongue == face || tongue == out.tooth.parent)
    throw std::runtime_error("Missing or duplicate tongue");
  out.tongue_geometry = private_geometry(out.actor, base, nodes[tongue]);
  out.tongue = wrap(tongue);
  for (size_t i = 0; i < 9; ++i) {
    auto eye = search(mask[0x37 + i]);
    if (eye != none)
      out.eyes[i] = wrap(eye);
  }
  if (nodes.size() != count + 4)
    throw std::runtime_error(
        "Unsupported facial skin expansion (requires four added objects)");
  validate_mask_tables(mask, out.face_geometry.vertex_count,
                       out.tongue_geometry.vertex_count);
  Reader prepared{out.actor.model};
  auto jaw0 = mr.u16(8), jaw1 = mr.u16(10);
  if (jaw0 >= out.face_geometry.vertex_count ||
      jaw1 >= out.face_geometry.vertex_count)
    throw std::runtime_error("Jaw anchor outside face");
  auto a = prepared.vec(out.face_geometry.source_vertices + 64 +
                        size_t(jaw0) * 32),
       b = prepared.vec(out.face_geometry.source_vertices + 64 +
                        size_t(jaw1) * 32);
  float x = b.x - a.x, y = b.y - a.y, z = b.z - a.z;
  float ry = std::atan(y / x), rz = std::atan(z / std::sqrt(x * x + y * y));
  if (!std::isfinite(ry) || !std::isfinite(rz))
    throw std::runtime_error("Degenerate jaw anchors");
  constexpr float angular = 65536.0f / 6.2831853071795864769f;
  out.jaw_anchor = a;
  out.jaw_angles = {int32_t(angular * rz), int32_t(angular * ry)};
  for (auto part : {out.tooth, out.tongue}) {
    Bytes bytes(nodes[part.parent].bytes.begin(),
                nodes[part.parent].bytes.end());
    put(bytes, 24, uint32_t(out.jaw_angles[0]));
    put(bytes, 28, uint32_t(out.jaw_angles[1]));
    std::copy(bytes.begin(), bytes.end(), nodes[part.parent].bytes.begin());
  }
  std::vector<size_t> order;
  std::vector<size_t> mapping(nodes.size());
  std::function<void(uint32_t)> flatten = [&](uint32_t i) {
    if (i == none)
      return;
    mapping[i] = order.size();
    order.push_back(i);
    flatten(nodes[i].child);
    flatten(nodes[i].sibling);
  };
  flatten(0);
  if (order.size() != nodes.size())
    throw std::runtime_error("Unreachable facial object");
  size_t new_table = out.actor.model.size();
  if (new_table + nodes.size() * 52 > 64 * 1024 * 1024)
    throw std::runtime_error("Prepared facial model exceeds limit");
  out.actor.model.resize(new_table + nodes.size() * 52);
  auto offset = [&](uint32_t i) {
    return i == none ? none : uint32_t(new_table - base + mapping[i] * 52);
  };
  for (size_t i = 0; i < order.size(); ++i) {
    auto &n = nodes[order[i]];
    size_t p = new_table + i * 52;
    std::copy(n.bytes.begin(), n.bytes.end(), out.actor.model.begin() + p);
    put(out.actor.model, p + 4, n.model);
    put(out.actor.model, p + 44, offset(n.child));
    put(out.actor.model, p + 48, offset(n.sibling));
  }
  half(out.actor.model, 6, uint16_t(nodes.size()));
  put(out.actor.model, 12, uint32_t(new_table - base));
  out.original_to_runtime.assign(mapping.begin(), mapping.begin() + count);
  out.face_joint = mapping[face];
  auto remap = [&](FacePart &p) {
    p.parent = mapping[p.parent];
    p.clone = mapping[p.clone];
  };
  remap(out.tooth);
  remap(out.tongue);
  for (auto &eye : out.eyes)
    if (eye)
      remap(*eye);
  out.actor.skin_layout = SkinLayout::FaceModel;
  out.actor.skin_preconverted = true;
  decode_skin(out.actor.skin, nodes.size(), SkinLayout::FaceModel);
  out.work.joints.resize(nodes.size());
  for (size_t i = 6; i <= 14; ++i)
    out.work.joints[i].flags |= 4;
  actor_joint_matrices(out.actor, {});
  return out;
}
} // namespace ashford
