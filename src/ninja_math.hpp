#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace ashford {
// ps2_NaMath.c::_Make_SinTable/njSinCos, including the retail 16383 reflection.
inline std::array<float, 2> ninja_sin_cos(uint16_t angle) {
  static const auto table = [] {
    std::array<float, 16384> values{};
    for (size_t i = 0; i < values.size(); ++i)
      values[i] = std::sin(0.0000958738f * float(i));
    return values;
  }();
  size_t index = angle & 16383;
  float sine = table[(angle & 0x4000) ? 16383 - index : index];
  if (angle & 0x8000)
    sine = -sine;
  float cosine = table[(angle & 0x4000) ? index : 16383 - index];
  if (bool(angle & 0x4000) != bool(angle & 0x8000))
    cosine = -cosine;
  return {sine, cosine};
}
} // namespace ashford
