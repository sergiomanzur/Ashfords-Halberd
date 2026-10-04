#pragma once
#include <array>
#include <cstdint>
namespace effect_alpha_reference {
// Unchanged scalar source helpers, native C capture; not PS2/gameplay
// acceptance.
struct Case {
  uint8_t original, alpha;
  bool black;
  uint8_t gs_alpha;
};
inline constexpr std::array<Case, 80> cases{{
    {0, 0, false, 0},     {0, 0, true, 0},      {0, 1, false, 1},
    {0, 1, true, 0},      {0, 15, false, 15},   {0, 15, true, 0},
    {0, 16, false, 16},   {0, 16, true, 0},     {0, 31, false, 31},
    {0, 31, true, 0},     {0, 32, false, 32},   {0, 32, true, 0},
    {0, 127, false, 127}, {0, 127, true, 0},    {0, 128, false, 128},
    {0, 128, true, 0},    {0, 240, false, 240}, {0, 240, true, 0},
    {0, 255, false, 255}, {0, 255, true, 0},    {1, 0, false, 0},
    {1, 0, true, 0},      {1, 1, false, 1},     {1, 1, true, 0},
    {1, 15, false, 15},   {1, 15, true, 0},     {1, 16, false, 16},
    {1, 16, true, 0},     {1, 31, false, 31},   {1, 31, true, 0},
    {1, 32, false, 32},   {1, 32, true, 0},     {1, 127, false, 127},
    {1, 127, true, 0},    {1, 128, false, 128}, {1, 128, true, 0},
    {1, 240, false, 240}, {1, 240, true, 0},    {1, 255, false, 255},
    {1, 255, true, 0},    {2, 0, false, 0},     {2, 0, true, 0},
    {2, 1, false, 1},     {2, 1, true, 0},      {2, 15, false, 8},
    {2, 15, true, 0},     {2, 16, false, 9},    {2, 16, true, 0},
    {2, 31, false, 16},   {2, 31, true, 0},     {2, 32, false, 17},
    {2, 32, true, 0},     {2, 127, false, 64},  {2, 127, true, 0},
    {2, 128, false, 68},  {2, 128, true, 0},    {2, 240, false, 128},
    {2, 240, true, 0},    {2, 255, false, 128}, {2, 255, true, 0},
    {6, 0, false, 0},     {6, 0, true, 0},      {6, 1, false, 1},
    {6, 1, true, 0},      {6, 15, false, 8},    {6, 15, true, 0},
    {6, 16, false, 8},    {6, 16, true, 0},     {6, 31, false, 16},
    {6, 31, true, 0},     {6, 32, false, 16},   {6, 32, true, 0},
    {6, 127, false, 64},  {6, 127, true, 0},    {6, 128, false, 64},
    {6, 128, true, 0},    {6, 240, false, 120}, {6, 240, true, 0},
    {6, 255, false, 128}, {6, 255, true, 0},
}};
} // namespace effect_alpha_reference
