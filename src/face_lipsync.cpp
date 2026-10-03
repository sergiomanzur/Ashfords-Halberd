#include "face_lipsync.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
// MIT recvx-decomp face_bh.c: fmSetLipSyncParam and fmCnkSet*ParamLip.
namespace ashford {
namespace {
constexpr float angle_scale = 0.005493164f;
uint32_t word(const Bytes &b, size_t p) {
  return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
         uint32_t(b[p + 3]) << 24;
}
int32_t signed_word(const Bytes &b, size_t p) {
  const auto bits = word(b, p);
  int32_t value;
  std::memcpy(&value, &bits, 4);
  return value;
}
float scalar(const Bytes &b, size_t p) {
  const auto bits = word(b, p);
  float value;
  std::memcpy(&value, &bits, 4);
  if (!std::isfinite(value))
    throw std::runtime_error("Non-finite lip parameter");
  return value;
}
FaceKeyframe key(const LipShape &shape) {
  FaceKeyframe result;
  result.parameters.muscle = shape.muscle;
  result.parameters.jaw_translation = shape.jaw_translation;
  result.parameters.tongue.z = shape.tongue_z;
  result.interpolation = shape.interpolation;
  return result;
}
float angle(int32_t first, int32_t second, float rate, float level) {
  const int64_t difference = int64_t(second) - first;
  if (difference < std::numeric_limits<int32_t>::min() ||
      difference > std::numeric_limits<int32_t>::max())
    throw std::runtime_error("Lip angle difference exceeds signed range");
  // Preserve source multiplication order: convert the integer difference,
  // rather than subtracting two already rounded degree values.
  const float result = level * (angle_scale * first +
                                rate * (angle_scale * int32_t(difference)));
  if (!std::isfinite(result))
    throw std::runtime_error("Non-finite lip angle");
  return result;
}
} // namespace
std::vector<LipShape> decode_lip_shapes(const Bytes &bytes) {
  if (bytes.empty() || bytes.size() % 164 || bytes.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Invalid packed lip shape extent");
  std::vector<LipShape> shapes;
  shapes.reserve(bytes.size() / 164);
  for (size_t p = 0; p < bytes.size(); p += 164) {
    LipShape s;
    for (size_t i = 0; i < 32; ++i)
      s.muscle[i] = scalar(bytes, p + i * 4);
    s.jaw_angle = signed_word(bytes, p + 128);
    s.jaw_translation = scalar(bytes, p + 132);
    s.tongue_x = signed_word(bytes, p + 148);
    s.tongue_y = signed_word(bytes, p + 152);
    s.tongue_z = scalar(bytes, p + 156);
    const uint16_t flag = uint16_t(bytes[p + 162]) | uint16_t(bytes[p + 163])
                                                         << 8;
    std::memcpy(&s.interpolation, &flag, 2);
    shapes.push_back(s);
  }
  return shapes;
}
std::vector<Bytes> decode_room_lip_clips(const Bytes &room) {
  if (room.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Room lip input exceeds limit");
  const auto info = room_info(room);
  if (info.enemies > 128)
    throw std::runtime_error("Too many room lip motion pointers");
  auto read = [&](size_t p) {
    if (p > room.size() || room.size() - p < 4)
      throw std::runtime_error("Room lip pointer outside input");
    return word(room, p);
  };
  const size_t table = read(24), count = size_t(info.enemies) + 3;
  if (!table)
    return {};
  if (table < 36 || table > room.size() || count * 4 > room.size() - table)
    throw std::runtime_error("Room lip pointer table outside input");
  size_t p = read(table + (count - 1) * 4);
  if (!p)
    return {};
  if (p < table + count * 4 || p >= room.size())
    throw std::runtime_error("Room lip bank overlaps pointer table");
  size_t end = room.size();
  for (size_t i = 0; i < count; ++i) {
    const size_t pointer = read(table + i * 4);
    if (pointer > p && pointer < end)
      end = pointer;
  }
  for (size_t header : {16u, 20u, 24u, 28u, 32u}) {
    const size_t pointer = read(header);
    if (pointer > p && pointer < end)
      end = pointer;
  }
  std::vector<Bytes> clips;
  for (;;) {
    if (end - p < 4)
      throw std::runtime_error("Room lip bank has no terminator");
    const auto size = read(p);
    p += 4;
    if (size == 0xffffffff)
      return clips;
    if (size > end - p || clips.size() >= 65536)
      throw std::runtime_error("Room lip clip exceeds extent or limit");
    clips.emplace_back(room.begin() + p, room.begin() + p + size);
    p += size;
  }
}
LipPlayback start_lip_playback(const Bytes &clip, size_t shape_count) {
  if (!shape_count || clip.empty() || clip.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Invalid lip clip extent");
  size_t p = 0;
  while (p < clip.size() && clip[p] != 255) {
    if (clip.size() - p < 2 || clip[p] >= shape_count || !clip[p + 1])
      throw std::runtime_error("Invalid lip clip segment");
    p += 2;
  }
  if (p == clip.size())
    throw std::runtime_error("Lip clip has no terminator");
  LipPlayback playback;
  playback.segments.reserve(p / 2);
  for (size_t i = 0; i < p; i += 2)
    playback.segments.push_back({clip[i], clip[i + 1]});
  playback.active = true;
  return playback;
}
void tick_lip_playback(FaceParameters &parameters,
                       const std::vector<LipShape> &shapes,
                       LipPlayback &playback, uint32_t mask) {
  if (!playback.active)
    return;
  if (shapes.empty() || playback.cursor > playback.segments.size() ||
      playback.count < 0 || playback.time < 0 || playback.time > 255)
    throw std::runtime_error("Invalid lip playback state");
  // Work on scalar state locally; avoid copying the entire clip each tick.
  auto cursor = playback.cursor;
  int count = playback.count, time = playback.time;
  auto current = playback.current, next = playback.next;
  bool active = playback.active;
  if (count >= time) {
    count -= time;
    if (cursor == playback.segments.size()) {
      current = 0;
      active = false;
    } else {
      const auto segment = playback.segments[cursor++];
      if (!segment.duration)
        throw std::runtime_error("Zero lip duration");
      current = segment.shape;
      time = segment.duration;
      next = cursor == playback.segments.size()
                 ? 0
                 : playback.segments[cursor].shape;
    }
  }
  if (current >= shapes.size() || next >= shapes.size())
    throw std::runtime_error("Lip shape index exceeds bank");
  const bool blend = active && time - count < 10;
  float t = 0, level = 1;
  if (blend) {
    if (time < 10) {
      t = float(count) / time;
      level = time / 10.0f;
    } else
      t = (10 - (time - count)) / 10.0f;
  }
  const auto &a = shapes[current], &b = shapes[blend ? next : current];
  const auto saved_eye = parameters.eye;
  // Direct setter ignores interpolation mode (including when count reaches FF).
  auto first = key(a);
  if (!blend)
    first.interpolation = 0;
  auto result =
      interpolate_face_parameters(parameters, first, key(b), t, level, mask);
  result.eye = saved_eye;
  const float rate = blend ? face_interpolation_rate(a.interpolation, t) : 0;
  result.jaw_angle = angle(a.jaw_angle, b.jaw_angle, rate, level);
  result.tongue.x = angle(a.tongue_x, b.tongue_x, rate, level);
  result.tongue.y = angle(a.tongue_y, b.tongue_y, rate, level);
  if (active)
    count += 2;
  if (count > std::numeric_limits<int16_t>::max())
    throw std::runtime_error("Lip counter exceeds range");
  parameters = result;
  playback.cursor = cursor;
  playback.count = int16_t(count);
  playback.time = int16_t(time);
  playback.current = current;
  playback.next = next;
  playback.active = active;
}
} // namespace ashford
