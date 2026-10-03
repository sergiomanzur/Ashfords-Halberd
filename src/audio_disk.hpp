#pragma once
#include "assets.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ashford::audio_detail {
// A bounded view; every nested chunk is checked against its own declared size.
class Disk {
public:
  explicit Disk(const Bytes &bytes) : bytes_(bytes), size_(bytes.size()) {
    if (size_ > 64 * 1024 * 1024)
      throw std::runtime_error("Sound asset exceeds size limit");
  }
  size_t size() const { return size_; }
  void range(size_t at, size_t count) const {
    if (at > size_ || count > size_ - at)
      throw std::runtime_error("Sound asset range exceeds input");
  }
  uint8_t u8(size_t at) const {
    range(at, 1);
    return bytes_[begin_ + at];
  }
  uint16_t u16(size_t at) const {
    range(at, 2);
    return uint16_t(u8(at)) | uint16_t(u8(at + 1)) << 8;
  }
  uint32_t u32(size_t at) const {
    range(at, 4);
    return uint32_t(u16(at)) | uint32_t(u16(at + 2)) << 16;
  }
  void tag(size_t at, const char *expected) const {
    range(at, 8);
    for (size_t i = 0; i < 8; ++i)
      if (u8(at + i) != uint8_t(expected[i]))
        throw std::runtime_error("Invalid sound chunk signature");
  }
  Disk view(size_t at, size_t count) const {
    range(at, count);
    return Disk(bytes_, begin_ + at, count);
  }
  Bytes copy(size_t at, size_t count) const {
    range(at, count);
    return Bytes(bytes_.begin() + begin_ + at,
                 bytes_.begin() + begin_ + at + count);
  }

private:
  Disk(const Bytes &bytes, size_t begin, size_t count)
      : bytes_(bytes), begin_(begin), size_(count) {}
  const Bytes &bytes_;
  size_t begin_ = 0, size_ = 0;
};
inline Disk chunk(const Disk &disk, size_t at, const char *signature) {
  disk.range(at, 16);
  disk.tag(at, signature);
  const auto size = disk.u32(at + 8);
  if (size < 16)
    throw std::runtime_error("Invalid sound chunk size");
  return disk.view(at, size);
}
inline size_t table_count(const Disk &disk) {
  const auto maximum = disk.u32(12);
  if (maximum >= 4096)
    throw std::runtime_error("Sound table count exceeds limit");
  const size_t count = size_t(maximum) + 1;
  disk.range(16, count * 4);
  return count;
}
inline size_t parameter(const Disk &disk, size_t count, size_t index,
                        size_t minimum) {
  const auto at = disk.u32(16 + index * 4);
  if (at == 0xffffffffu)
    return at;
  if (at < 16 + count * 4)
    throw std::runtime_error("Sound parameter overlaps index table");
  disk.range(at, minimum);
  return at;
}
inline void seven_bit(uint8_t value) {
  if (value > 127)
    throw std::runtime_error("Invalid sound seven-bit parameter");
}
} // namespace ashford::audio_detail
