#include "face_deformation.hpp"
#include "face_math.hpp"
// MIT recvx-decomp face_bh.c _fmCnkCalcMuscle/Jaw/Tang/Eye.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ashford {
namespace {
struct Reader {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Facial deformation extent exceeds model");
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
      throw std::runtime_error("Non-finite facial deformation value");
    return f;
  }
  Vec3 vec(size_t p) const { return {f32(p), f32(p + 4), f32(p + 8)}; }
};
void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
void write(Bytes &b, size_t p, float f) {
  if (!std::isfinite(f))
    throw std::runtime_error("Facial deformation overflow");
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
void write_point(Bytes &b, size_t p, Vec3 v) {
  write(b, p, v.x);
  write(b, p + 4, v.y);
  write(b, p + 8, v.z);
}
size_t vertex(size_t buffer, size_t index) { return buffer + 64 + index * 32; }
int32_t angle_value(float radians_or_degrees, float factor) {
  float v = radians_or_degrees * factor;
  if (!std::isfinite(v) || double(v) < std::numeric_limits<int32_t>::min() ||
      double(v) > std::numeric_limits<int32_t>::max())
    throw std::runtime_error("Facial rotation exceeds range");
  return int32_t(v);
}
uint16_t angle(float value, float factor) {
  return uint16_t(angle_value(value, factor));
}
std::array<float, 40> values(const FaceParameters &p) {
  std::array<float, 40> v{};
  std::copy(p.muscle.begin(), p.muscle.end(), v.begin());
  v[32] = p.jaw_angle;
  v[33] = p.jaw_translation;
  v[34] = p.eye.x;
  v[35] = p.eye.y;
  v[36] = p.eye.z;
  v[37] = p.tongue.x;
  v[38] = p.tongue.y;
  v[39] = p.tongue.z;
  for (float f : v)
    if (!std::isfinite(f))
      throw std::runtime_error("Non-finite facial parameter");
  return v;
}
void validate_geometry(const Reader &r, const FaceGeometry &g) {
  if (g.vertex_count > 65535)
    throw std::runtime_error("Invalid facial vertex count");
  size_t length = 64 + g.vertex_count * 32;
  r.range(g.source_vertices, length);
  r.range(g.destination_vertices, length);
  if (g.source_vertices < g.destination_vertices + length &&
      g.destination_vertices < g.source_vertices + length)
    throw std::runtime_error("Facial source/destination overlap");
  if (r.b[g.source_vertices] != 51 || r.b[g.destination_vertices] != 51 ||
      r.u16(g.source_vertices + 6) != g.vertex_count ||
      r.u16(g.destination_vertices + 6) != g.vertex_count)
    throw std::runtime_error("Invalid prepared facial vertex buffers");
}
void validate_data(const FaceDeformationData &data,
                   const PreparedFaceModel &face) {
  if (data.muscles.size() > 1000000 || data.jaw.size() > 1000000 ||
      data.tongue.size() > 1000000)
    throw std::runtime_error("Facial deformation count exceeds limit");
  for (const auto &binding : data.muscles) {
    if (binding.vertex >= face.face_geometry.vertex_count ||
        binding.connections.size() > 32)
      throw std::runtime_error("Invalid facial muscle binding");
    for (const auto &c : binding.connections)
      if (c.parameter >= 40 || !std::isfinite(c.scale) ||
          !std::isfinite(c.vector.x) || !std::isfinite(c.vector.y) ||
          !std::isfinite(c.vector.z))
        throw std::runtime_error("Invalid facial muscle connection");
    for (auto index : binding.normal_vertices)
      if (index >= face.face_geometry.vertex_count)
        throw std::runtime_error("Invalid facial normal reference");
  }
  for (const auto &entry : data.jaw)
    if (entry.vertex >= face.face_geometry.vertex_count ||
        !std::isfinite(entry.rate))
      throw std::runtime_error("Invalid facial jaw binding");
  for (const auto &entry : data.tongue)
    if (entry.vertex >= face.tongue_geometry.vertex_count ||
        !std::isfinite(entry.rate))
      throw std::runtime_error("Invalid facial tongue binding");
}
} // namespace
FaceDeformationData decode_face_deformation(const Bytes &mask,
                                            const PreparedFaceModel &face) {
  if (mask.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Facial MASK exceeds limit");
  Reader r{mask};
  r.range(0, 64);
  if (std::string(mask.begin(), mask.begin() + 4) != "MASK")
    throw std::runtime_error("Invalid facial deformation signature");
  auto lists = r.u32(0x10), connections = r.u32(0x14), jaw = r.u32(0x18),
       tongue = r.u32(0x1c), shapes = r.u32(0x20);
  for (auto count : {lists, connections, jaw, tongue, shapes})
    if (count > 1000000)
      throw std::runtime_error("Facial deformation count exceeds limit");
  size_t p = 64;
  r.range(p, size_t(lists) * 4);
  size_t normal_count = 0, connection_count = 0;
  for (size_t i = 0; i < lists; ++i) {
    auto id = int16_t(r.u16(p + i * 4));
    auto m = mask[p + i * 4 + 2], n = mask[p + i * 4 + 3];
    if (id < 0 || size_t(id) >= face.face_geometry.vertex_count || m > 32 ||
        n > 127)
      throw std::runtime_error("Invalid signed facial list fields");
    normal_count += n;
    connection_count += m;
  }
  if (connection_count > connections)
    throw std::runtime_error("Facial connection lists exceed table");
  // Prove every aggregate extent before allocating any native list/vector.
  size_t end = p + size_t(lists) * 4;
  for (size_t length :
       {normal_count * 4, size_t(connections) * 20, size_t(jaw) * 8,
        size_t(tongue) * 8, size_t(shapes) * 164}) {
    r.range(end, length);
    end += length;
  }
  FaceDeformationData data;
  data.muscles.resize(lists);
  for (size_t i = 0; i < lists; ++i) {
    auto id = int16_t(r.u16(p + i * 4));
    auto m = mask[p + i * 4 + 2], n = mask[p + i * 4 + 3];
    if (id < 0 || m > 32 || n > 127)
      throw std::runtime_error("Invalid signed facial list fields");
    data.muscles[i].vertex = size_t(id);
    data.muscles[i].connections.resize(m);
    data.muscles[i].normal_vertices.resize(n);
  }
  p += size_t(lists) * 4;
  for (auto &binding : data.muscles)
    for (auto &index : binding.normal_vertices) {
      index = r.u32(p);
      p += 4;
    }
  r.range(p, size_t(connections) * 20);
  size_t c = 0;
  for (auto &binding : data.muscles)
    for (auto &connection : binding.connections) {
      if (c >= connections)
        throw std::runtime_error("Facial connection lists exceed table");
      size_t q = p + c++ * 20;
      connection = {r.u32(q), r.vec(q + 4), r.f32(q + 16)};
    }
  p += size_t(connections) * 20;
  auto weights = [&](size_t count, std::vector<FaceWeightedVertex> &out) {
    r.range(p, count * 8);
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      out.push_back({r.u32(p), r.f32(p + 4)});
      p += 8;
    }
  };
  weights(jaw, data.jaw);
  weights(tongue, data.tongue);
  r.range(p, size_t(shapes) * 164);
  validate_data(data, face);
  return data;
}
void deform_face(PreparedFaceModel &face, const FaceDeformationData &data,
                 const FaceParameters &parameters) {
  using namespace face_math;
  if (face.actor.skin_layout != SkinLayout::FaceModel ||
      !face.actor.skin_preconverted)
    throw std::runtime_error("Facial deformation requires prepared model");
  const auto params = values(parameters);
  Reader source{face.actor.model};
  validate_geometry(source, face.face_geometry);
  validate_geometry(source, face.tongue_geometry);
  validate_data(data, face);
  const auto pose = actor_bind_pose(face.actor);
  size_t base = source.u16(4), table = base + size_t(source.u32(12));
  if (face.tooth.parent >= pose.size() || face.tongue.parent >= pose.size())
    throw std::runtime_error("Invalid facial parent binding");
  Bytes next = face.actor.model;
  const auto &fg = face.face_geometry, &tg = face.tongue_geometry;
  auto reset = [&](size_t i) {
    size_t src = vertex(fg.source_vertices, i),
           dst = vertex(fg.destination_vertices, i);
    std::copy_n(face.actor.model.begin() + src, 16, next.begin() + dst);
  };
  for (const auto &entry : data.jaw)
    reset(entry.vertex);
  for (const auto &binding : data.muscles) {
    reset(binding.vertex);
    float sum = 0;
    struct Active {
      const FaceConnection *connection;
      float coefficient;
    };
    std::vector<Active> active;
    for (const auto &c : binding.connections) {
      float parameter = params[c.parameter];
      if (parameter != 0) {
        float rate = c.scale * std::abs(parameter);
        sum += rate;
        active.push_back({&c, parameter * rate});
      }
    }
    auto p = source.vec(vertex(fg.source_vertices, binding.vertex));
    if (!active.empty()) {
      float inverse = 1.0f / sum;
      // PS2 assembly consumes the scratch connection pointers backwards.
      for (auto i = active.rbegin(); i != active.rend(); ++i) {
        float rate = i->coefficient * inverse;
        p.x += i->connection->vector.x * rate;
        p.y += i->connection->vector.y * rate;
        p.z += i->connection->vector.z * rate;
      }
    }
    write_point(next, vertex(fg.destination_vertices, binding.vertex), p);
  }
  auto jmat1 = multiply(multiply(rotation(1, uint16_t(-face.jaw_angles[0])),
                                 rotation(2, uint16_t(-face.jaw_angles[1]))),
                        translation({-face.jaw_anchor.x, -face.jaw_anchor.y,
                                     -face.jaw_anchor.z}));
  auto jmat2 = multiply(multiply(translation(face.jaw_anchor),
                                 rotation(2, uint16_t(face.jaw_angles[1]))),
                        rotation(1, uint16_t(face.jaw_angles[0])));
  if (!data.jaw.empty()) {
    const auto jaw_angle = angle(-parameters.jaw_angle, 182.04445f);
    for (const auto &entry : data.jaw) {
      auto m = multiply(
          jmat2,
          rotation(0, angle(-parameters.jaw_angle * entry.rate, 182.04445f)));
      // Fractional rates scale the rotation, not the translation.
      m = multiply(m, translation({parameters.jaw_translation, 0, 0}));
      m = multiply(m, jmat1);
      size_t p = vertex(fg.destination_vertices, entry.vertex);
      write_point(next, p, point(m, Reader{next}.vec(p)));
    }
    put(next, table + face.tooth.parent * 52 + 20, jaw_angle);
    put(next, table + face.tongue.parent * 52 + 20, jaw_angle);
  }
  for (const auto &entry : data.tongue) {
    auto m = multiply(
        rotation(0, angle(parameters.tongue.x * entry.rate, 182.04445f)),
        rotation(1, angle(parameters.tongue.y * entry.rate, 182.04445f)));
    auto p = source.vec(vertex(tg.source_vertices, entry.vertex));
    p.z *= parameters.tongue.z;
    write_point(next, vertex(tg.destination_vertices, entry.vertex),
                point(m, p));
  }
  for (size_t i = 0; i < face.eyes.size(); ++i)
    if (face.eyes[i]) {
      if (!face.eye_matrices[i] || face.eyes[i]->parent >= pose.size())
        throw std::runtime_error("Invalid facial eye binding");
      const auto &m = *face.eye_matrices[i];
      for (double v : m)
        if (!std::isfinite(v))
          throw std::runtime_error("Non-finite facial eye matrix");
      size_t p = table + face.eyes[i]->parent * 52;
      auto origin = point(m, source.vec(p + 8));
      Vec3 q{(origin.x + parameters.eye.x) - origin.x,
             (origin.y + parameters.eye.y) - origin.y,
             (origin.z + parameters.eye.z) - origin.z};
      auto reference = point(m, {0, 0, -100});
      float pitch = std::atan(q.y / std::sqrt(q.x * q.x + q.z * q.z));
      int32_t ax = angle_value(pitch, 10430.381f);
      if (reference.z >= 0)
        ax = -ax;
      auto ay =
          angle_value(std::atan(q.x / q.z), 10430.381f) + (q.z < 0 ? 0 : 32768);
      put(next, p + 20, uint32_t(ax));
      put(next, p + 24, uint32_t(ay));
      put(next, p + 28, 0);
    }
  // Only mode 1 is requested by bhInitMask: normals retain original values.
  face.actor.model = std::move(next);
}
} // namespace ashford
