#include "motion.hpp"
// Adapted from MIT recvx-decomp binfunc.c::bhMnbBinRealize and
// Motion.c::SetMtnNormal/SetMtnFast/SetMtnSlow. Disk pointers stay offsets.
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ashford {
namespace {
struct Reader {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Motion extent exceeds input");
  }
  uint16_t u16(size_t p) const {
    range(p, 2);
    return uint16_t(b[p]) | uint16_t(b[p + 1]) << 8;
  }
  uint32_t u32(size_t p) const {
    return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16;
  }
  float f32(size_t p) const {
    auto bits = u32(p);
    float f;
    std::memcpy(&f, &bits, 4);
    if (!std::isfinite(f))
      throw std::runtime_error("Non-finite motion key");
    return f;
  }
};
} // namespace
int advance_motion_frame(uint32_t frames, int32_t step, uint32_t mode,
                         int32_t &frame, uint32_t &direction) {
  if (!frames || frames > 65535)
    throw std::runtime_error("Invalid motion frame count");
  auto signed_word = [](uint32_t bits) {
    return bits < 0x80000000u ? int32_t(bits)
                              : int32_t(int64_t(bits) - 0x100000000ll);
  };
  uint32_t extent = frames << 16;
  uint32_t delta = uint32_t(step);
  frame = signed_word((mode & 1) && direction ? uint32_t(frame) - delta
                                              : uint32_t(frame) + delta);
  if (mode & 1) {
    if (frame <= 0 || frame >= signed_word(extent - 65536))
      direction = ~direction;
  } else {
    if (frame < 0) {
      frame = signed_word(uint32_t(frame) + extent);
      return -1;
    }
    if (frame >= signed_word(extent)) {
      frame = signed_word(uint32_t(frame) - extent);
      return 1;
    }
  }
  return 0;
}
std::vector<std::optional<Motion>> decode_motion_bank(const Bytes &data,
                                                      size_t begin, size_t end,
                                                      size_t key_limit) {
  if (begin > end || end > data.size() || end - begin > 64 * 1024 * 1024)
    throw std::runtime_error("Invalid motion bank extent");
  Reader r{data};
  std::vector<std::optional<Motion>> result;
  size_t p = begin;
  for (;;) {
    if (end - p < 4)
      throw std::runtime_error("Missing motion bank terminator");
    uint32_t size = r.u32(p);
    p += 4;
    if (size == 0xffffffff)
      return result;
    if (result.size() >= 512 || size > end - p)
      throw std::runtime_error("Motion bank record outside limit");
    if (!size)
      result.push_back(std::nullopt);
    else {
      auto motion = decode_motion(
          Bytes(data.begin() + p, data.begin() + p + size), key_limit);
      for (const auto &track : motion.tracks)
        key_limit -= track.positions.size() + track.angles.size();
      result.push_back(std::move(motion));
    }
    p += size;
  }
}
std::vector<std::optional<Motion>> decode_room_motion_bank(const Bytes &data,
                                                           size_t key_limit) {
  auto info = room_info(data);
  Reader r{data};
  size_t table = r.u32(24);
  if (!table)
    return std::vector<std::optional<Motion>>(512);
  if (info.enemies > 128)
    throw std::runtime_error("Too many room enemy motion pointers");
  size_t count = size_t(info.enemies) + 3;
  r.range(table, count * 4);
  size_t table_end = table + count * 4;
  size_t begin = r.u32(table + size_t(info.enemies) * 4);
  if (!begin)
    return std::vector<std::optional<Motion>>(512);
  if (begin < table_end || begin >= data.size())
    throw std::runtime_error("Room motion bank overlaps pointer table");
  size_t end = data.size();
  for (size_t i = 0; i < count; ++i) {
    size_t pointer = r.u32(table + i * 4);
    if (pointer > begin && pointer < end)
      end = pointer;
  }
  for (size_t header :
       {size_t(16), size_t(20), size_t(24), size_t(28), size_t(32)}) {
    size_t pointer = r.u32(header);
    if (pointer > begin && pointer < end)
      end = pointer;
  }
  auto result = decode_motion_bank(data, begin, end, key_limit);
  result.resize(512);
  return result;
}
Motion decode_motion(const Bytes &data, size_t key_limit) {
  Reader r{data};
  r.range(0, 16);
  if (data[0] != 'M' || data[1] != 'T' || data[2] != 'N')
    throw std::runtime_error("Invalid motion signature");
  size_t base = r.u16(4);
  auto count = r.u16(6);
  if (base < 16 || count == 0 || count > 4096)
    throw std::runtime_error("Invalid motion header");
  r.range(0, base);
  size_t header = base + size_t(r.u32(8));
  r.range(header, 12);
  Motion m;
  m.flags = data[3];
  m.frames = r.u32(header + 4);
  if (m.frames == 0 || m.frames > 65535 || uint64_t(m.frames) * count > 2000000)
    throw std::runtime_error("Motion key limit exceeded");
  if (r.u16(header + 8) != 3 || r.u16(header + 10) != 2)
    throw std::runtime_error("Unsupported motion channel layout");
  size_t table = base + size_t(r.u32(header));
  r.range(table, size_t(count) * 8);
  size_t keys = 0;
  for (size_t i = 0; i < count; ++i)
    for (size_t channel = 0; channel < 2; ++channel)
      if (r.u32(table + i * 8 + channel * 4) != 0xffffffff) {
        if (m.frames > key_limit - keys)
          throw std::runtime_error("Decoded motion key budget exceeded");
        keys += m.frames;
      }
  m.tracks.resize(count);
  for (size_t i = 0; i < count; ++i) {
    auto p = r.u32(table + i * 8), a = r.u32(table + i * 8 + 4);
    auto &t = m.tracks[i];
    if (p != 0xffffffff) {
      size_t keys = base + size_t(p);
      r.range(keys, size_t(m.frames) * 12);
      t.positions.reserve(m.frames);
      for (size_t f = 0; f < m.frames; ++f)
        t.positions.push_back({r.f32(keys + f * 12), r.f32(keys + f * 12 + 4),
                               r.f32(keys + f * 12 + 8)});
    }
    if (a != 0xffffffff) {
      size_t keys = base + size_t(a);
      r.range(keys, size_t(m.frames) * 6);
      t.angles.reserve(m.frames);
      for (size_t f = 0; f < m.frames; ++f)
        t.angles.push_back({r.u16(keys + f * 6), r.u16(keys + f * 6 + 2),
                            r.u16(keys + f * 6 + 4)});
    }
  }
  auto attr = r.u32(12);
  if (attr != 0xffffffff) {
    size_t p = base + size_t(attr);
    r.range(p, size_t(m.frames) * 2);
    for (size_t f = 0; f < m.frames; ++f)
      m.attributes.push_back(r.u16(p + f * 2));
  }
  return m;
}
std::vector<JointPose> sample_motion(const Motion &m, int64_t frame,
                                     bool interpolate) {
  if (frame < 0 || !m.frames)
    throw std::runtime_error("Invalid motion frame");
  auto first = size_t((frame / 65536) % m.frames),
       next = (first + 1) % m.frames;
  float fraction = interpolate ? float(frame & 65535) / 65536.f : 0;
  std::vector<JointPose> pose(m.tracks.size());
  for (size_t i = 0; i < m.tracks.size(); ++i) {
    const auto &t = m.tracks[i];
    if (!t.positions.empty()) {
      if (t.positions.size() != m.frames)
        throw std::runtime_error("Invalid position track");
      auto a = t.positions[first], b = t.positions[next];
      pose[i].position =
          Vec3{a.x + fraction * (b.x - a.x), a.y + fraction * (b.y - a.y),
               a.z + fraction * (b.z - a.z)};
    }
    if (!t.angles.empty()) {
      if (t.angles.size() != m.frames)
        throw std::runtime_error("Invalid angle track");
      std::array<uint16_t, 3> out{};
      for (size_t axis = 0; axis < 3; ++axis) {
        int a = t.angles[first][axis], b = t.angles[next][axis];
        // SetMtnSlow casts the delta to signed 16-bit before interpolation.
        int delta = (b - a) & 65535;
        if (delta >= 32768)
          delta -= 65536;
        int signed_a = a >= 32768 ? a - 65536 : a;
        out[axis] = uint16_t(int(float(signed_a) + fraction * delta));
      }
      pose[i].angles = out;
    }
  }
  return pose;
}
} // namespace ashford
