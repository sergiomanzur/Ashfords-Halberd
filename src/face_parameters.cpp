#include "face_parameters.hpp"
#include "ninja_math.hpp"
// MIT recvx-decomp face_bh.c: fmCnkSetInterParam/fmCnkSetCurrentFrame.
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
      throw std::runtime_error("Facial track extent exceeds input");
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
      throw std::runtime_error("Non-finite facial parameter");
    return f;
  }
};
void finite(float v) {
  if (!std::isfinite(v))
    throw std::runtime_error("Non-finite facial parameter");
}
void validate(const FaceParameters &p) {
  for (float m : p.muscle)
    finite(m);
  for (float v : {p.jaw_angle, p.jaw_translation, p.eye.x, p.eye.y, p.eye.z,
                  p.tongue.x, p.tongue.y, p.tongue.z})
    finite(v);
}
void validate_track(const std::vector<FaceKeyframe> &track) {
  if (track.empty() || track.size() > 1000000)
    throw std::runtime_error("Invalid facial track count");
  int previous = -1;
  for (const auto &key : track) {
    if (key.frame < 0 || key.frame <= previous)
      throw std::runtime_error("Facial frames must increase");
    previous = key.frame;
    validate(key.parameters);
  }
}
} // namespace
Bytes face_track_records(const Bytes &mask) {
  if (mask.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Facial track exceeds limit");
  Reader r{mask};
  r.range(0, 64);
  if (std::string(mask.begin(), mask.begin() + 4) != "MASK")
    throw std::runtime_error("Invalid facial track signature");
  auto lists = r.u32(0x10), connections = r.u32(0x14), jaws = r.u32(0x18),
       tongues = r.u32(0x1c), shapes = r.u32(0x20);
  for (auto count : {lists, connections, jaws, tongues, shapes})
    if (count > 1000000)
      throw std::runtime_error("Facial track count exceeds limit");
  size_t p = 64, indices = 0;
  r.range(p, size_t(lists) * 4);
  for (size_t i = 0; i < lists; ++i) {
    auto n = mask[p + i * 4 + 3];
    if (n > 127)
      throw std::runtime_error("Invalid signed facial vertex count");
    indices += n;
  }
  p += size_t(lists) * 4;
  auto skip = [&](size_t n) {
    r.range(p, n);
    p += n;
  };
  skip(indices * 4);
  skip(size_t(connections) * 20);
  skip(size_t(jaws) * 8);
  skip(size_t(tongues) * 8);
  r.range(p, size_t(shapes) * 164);
  return Bytes(mask.begin() + p, mask.begin() + p + size_t(shapes) * 164);
}
std::vector<FaceKeyframe> decode_face_parameter_records(const Bytes &records) {
  if (records.size() % 164 || records.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Invalid facial parameter record extent");
  Reader r{records};
  const size_t shapes = records.size() / 164;
  std::vector<FaceKeyframe> track;
  track.reserve(shapes);
  for (size_t i = 0, p = 0; i < shapes; ++i, p += 164) {
    FaceKeyframe key;
    auto &v = key.parameters;
    for (size_t j = 0; j < 32; ++j)
      v.muscle[j] = r.f32(p + j * 4);
    v.jaw_angle = r.f32(p + 0x80);
    v.jaw_translation = r.f32(p + 0x84);
    v.eye = {r.f32(p + 0x88), r.f32(p + 0x8c), r.f32(p + 0x90)};
    v.tongue = {r.f32(p + 0x94), r.f32(p + 0x98), r.f32(p + 0x9c)};
    key.frame = int16_t(r.u16(p + 0xa0));
    key.interpolation = int16_t(r.u16(p + 0xa2));
    track.push_back(key);
  }
  // A zero-shape MASK can be prepared but cannot be sampled as a track.
  if (!track.empty())
    validate_track(track);
  return track;
}
std::vector<FaceKeyframe> decode_face_track(const Bytes &mask) {
  return decode_face_parameter_records(face_track_records(mask));
}
float face_interpolation_rate(int16_t mode, float t) {
  finite(t);
  float rate = t;
  switch (mode) {
  case 1:
    rate = t * t;
    break;
  case 2:
    rate = std::sqrt(t);
    break;
  case 3: {
    // njCos takes an integer angle; preserve its source truncation.
    const double angle = 32768.0f * t;
    if (angle < std::numeric_limits<int32_t>::min() ||
        angle > std::numeric_limits<int32_t>::max())
      throw std::runtime_error("Facial interpolation angle exceeds range");
    rate = 0.5f - 0.5f * ninja_sin_cos(uint16_t(int32_t(angle)))[1];
    break;
  }
  case 4:
    rate = 0;
    break;
  default:
    break;
  }
  finite(rate);
  return rate;
}
FaceParameters interpolate_face_parameters(FaceParameters out,
                                           const FaceKeyframe &a,
                                           const FaceKeyframe &b, float t,
                                           float level, uint32_t mask) {
  validate(out);
  validate(a.parameters);
  validate(b.parameters);
  finite(level);
  const float rate = face_interpolation_rate(a.interpolation, t);
  const auto &p = a.parameters, &q = b.parameters;
  auto blend = [&](float first, float second) {
    return first + rate * (second - first);
  };
  for (size_t i = 0; i < 32; ++i)
    if (mask & (uint32_t(1) << i))
      out.muscle[i] = blend(p.muscle[i], q.muscle[i]) * level;
  out.jaw_angle = blend(p.jaw_angle, q.jaw_angle) * level;
  out.jaw_translation = blend(p.jaw_translation, q.jaw_translation) * level;
  out.eye = {blend(p.eye.x, q.eye.x), blend(p.eye.y, q.eye.y),
             blend(p.eye.z, q.eye.z)};
  out.tongue = {blend(p.tongue.x, q.tongue.x) * level,
                blend(p.tongue.y, q.tongue.y) * level,
                blend(p.tongue.z, q.tongue.z) * level};
  validate(out);
  return out;
}
FaceParameters sample_face_track(const std::vector<FaceKeyframe> &track,
                                 float frame) {
  validate_track(track);
  finite(frame);
  if (frame < 0 || double(frame) > std::numeric_limits<uint32_t>::max())
    throw std::runtime_error("Invalid facial sample frame");
  size_t i = 0;
  const auto whole = uint32_t(frame);
  while (i < track.size() && uint32_t(track[i].frame) <= whole)
    ++i;
  if (i == 0)
    return track.front().parameters;
  if (i == track.size())
    return track.back().parameters;
  const auto &a = track[i - 1], &b = track[i];
  return interpolate_face_parameters(
      {}, a, b, (frame - a.frame) / float(b.frame - a.frame));
}
} // namespace ashford
