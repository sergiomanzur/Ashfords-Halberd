#pragma once
#include "assets.hpp"
#include <array>
namespace ashford {
enum class AmbientTarget : uint8_t { Room, Character, Object, Item };
using AmbientMaterial = std::array<uint8_t, 3>;
struct RoomAmbient {
  std::array<uint8_t, 4> selectors{};
  std::array<std::array<float, 3>, 4> colors{};
};
// ROM_WORK's four selectors and planar little-endian RGB float arrays.
RoomAmbient decode_room_ambient(const Bytes &data);
std::array<float, 3> selected_ambient(const RoomAmbient &, AmbientTarget);
} // namespace ashford
