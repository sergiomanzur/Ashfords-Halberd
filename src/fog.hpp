#pragma once
#include "assets.hpp"
#include <array>
#include <optional>
namespace ashford {
struct FogParameters {
  uint32_t color = 0;
  float near_z = 0, far_z = -100;
};
struct FogTable {
  float density = 0;
  std::array<float, 128> values{};
};
struct NativeFog {
  bool enabled = false;
  uint32_t color = 0;
  FogTable table;
};
struct FogWork {
  uint32_t override_color = 0;
  int32_t countdown = 0, change_ticks = 3;
  FogParameters camera;
  bool camera_enabled = false;
  NativeFog active;
};
FogParameters decode_room_fog(const Bytes &data);
FogTable generate_fog_table(float near_z, float far_z);
float fog_power(const NativeFog &fog, float inverse_z);
void select_fog_cut(FogWork &fog, const FogParameters &parameters, bool enabled,
                    uint32_t state_flags, uint32_t &game_flags,
                    uint32_t &effect_flags);
void select_fog_eye(FogWork &fog, FogParameters &room, uint32_t state_flags,
                    uint32_t &game_flags);
// sync.c::bhControlEOR. Keyframe mode must provide the actual keyframe8 flag.
void control_fog(FogWork &fog, uint32_t &state_flags, uint32_t &game_flags,
                 uint32_t camera_flags = 0,
                 std::optional<bool> keyframe_enabled = std::nullopt);
} // namespace ashford
