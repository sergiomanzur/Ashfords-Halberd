#pragma once
#include <array>
#include <cstdint>
namespace effect_palette_reference {
// Unchanged original ClutCopy/ClutCopy256 native C capture; not game
// acceptance.
inline constexpr std::array<uint32_t, 16> small{
    {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23}};
inline constexpr uint64_t large_hash = 7572307688389401475ull;
} // namespace effect_palette_reference
