#pragma once
#include "collision.hpp"
namespace ashford {
struct NativeEffectTemplate {
  uint32_t flags = 0;
  uint16_t id = 0, type = 0;
  uint16_t model_version = 0;
  int16_t floor = 0;
  Vec3 position{}, scale{};
  std::array<int16_t, 2> angles{}; // EF_WORK has only signed short ax/ay.
};
struct NativeEffectWork {
  uint32_t flags = 0;
  uint32_t state_flags = 0;
  uint16_t id = 0, type = 0;
  uint8_t model_version = 0;
  int8_t floor = 0;
  Vec3 position{}, scale{};
  std::array<int32_t, 3> angles{}; // O_WRK widens EF_WORK angles.
  int32_t texture_id = 0, linked_joint = 0;
  std::optional<size_t> linked_parent;
  Vec3 offset{}, base_scale{}, direction{};
  uint32_t motion_color = 0, motion_number = 0;
  std::optional<uint16_t> texture_slot;
  std::optional<size_t>
      texture_owner; // Model texture source, not linked parent.
  int32_t control0 = 0, control1 = 0; // O_WRK::ct0/ct1 birth counters.
  bool particle_sprite = false;
  std::optional<std::array<size_t, 2>> particle_storage;
};
struct NativeEffectPool {
  std::array<NativeEffectWork, 512> works{};
  NativeEffectTemplate scratch;      // sys->ef retains fields not overwritten.
  std::optional<size_t> ground_wall; // sys->htp, merged static/dynamic index.
};
} // namespace ashford
