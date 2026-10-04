#include "ambient.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp ROM_WORK and Ninja ambient inputs.
namespace ashford {
namespace {
uint32_t word(const Bytes &data, size_t p) {
  if (p > data.size() || data.size() - p < 4)
    throw std::runtime_error("Ambient record outside room data");
  return uint32_t(data[p]) | uint32_t(data[p + 1]) << 8 |
         uint32_t(data[p + 2]) << 16 | uint32_t(data[p + 3]) << 24;
}
std::array<float, 3> finite(std::array<float, 3> rgb) {
  for (float value : rgb)
    if (!std::isfinite(value))
      throw std::runtime_error("Nonfinite consumed room ambient color");
  return rgb;
}
} // namespace
RoomAmbient decode_room_ambient(const Bytes &data) {
  room_info(data);
  const size_t rom = word(data, 16);
  RoomAmbient result;
  for (size_t slot = 0; slot < 4; ++slot) {
    result.selectors[slot] = data.at(rom + 0x3b8 + slot);
    if (result.selectors[slot] >= 4)
      throw std::runtime_error("Ambient selector outside four room slots");
    for (size_t channel = 0; channel < 3; ++channel) {
      const auto bits = word(data, rom + 0x3bc + channel * 16 + slot * 4);
      std::memcpy(&result.colors[slot][channel], &bits, 4);
    }
    finite(result.colors[slot]);
  }
  return result;
}
std::array<float, 3> selected_ambient(const RoomAmbient &ambient,
                                      AmbientTarget target) {
  const auto group = size_t(target);
  if (group >= ambient.selectors.size() || ambient.selectors[group] >= 4)
    throw std::runtime_error("Invalid current room ambient selector");
  return finite(ambient.colors[ambient.selectors[group]]);
}
} // namespace ashford
