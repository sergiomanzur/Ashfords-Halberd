#pragma once
#include "collision.hpp"
#include "scene.hpp"
namespace ashford {
struct ScenarioState;
struct EnemyShadow;
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
  std::optional<ModelMatrix> model_matrix; // O_WRK::mtx after CalcLinkModel.
  std::optional<size_t> linked_wall; // Effect29's static/dynamic ATR link.
  Vec3 offset{}, base_scale{}, direction{};
  uint32_t motion_color = 0, motion_number = 0;
  std::optional<uint16_t> texture_slot;
  std::optional<size_t>
      texture_owner; // Model texture source, not linked parent.
  int32_t control0 = 0, control1 = 0; // O_WRK::ct0/ct1 birth counters.
  bool particle_sprite = false;
  std::optional<std::array<size_t, 2>> particle_storage;
  std::array<uint8_t, 4> modes{};
  std::array<int8_t, 15> shadow_joints{};
};
struct NativeEffectPool {
  std::array<NativeEffectWork, 512> works{};
  NativeEffectTemplate scratch;      // sys->ef retains fields not overwritten.
  std::optional<size_t> ground_wall; // sys->htp, merged static/dynamic index.
};
// effect.c control gate for wall timers, effsub1.c::bhEff029. Runs after player
// and before objects. Other particle playback remains a separate service.
void tick_wall_damage_timers(ScenarioState &state);
// room.c bhSetEffectTable / event.c bhEffDispSet. Authored links beyond0
// require their separate original model/link service and reject explicitly.
void bind_room_effects(const Bytes &data, ScenarioState &state);
void set_room_effect_visibility(ScenarioState &state, size_t authored,
                                bool visible);
// event.c bhEffModeSet assigns O_WRK::mode1 through sys->efid.
void set_room_effect_mode(ScenarioState &state, size_t authored, uint8_t mode);
// effect.c bhSetShadow / event.c bhInitSetKage / effsub1.c bhEff001.
// Uses real shared pool slots; modifier-volume drawing remains separate.
std::optional<size_t> create_enemy_shadow(ScenarioState &state, size_t work,
                                          const EnemyShadow &shadow);
void request_enemy_shadow(ScenarioState &state, size_t authored,
                          uint8_t variant);
void tick_shadow_effects(ScenarioState &state);
} // namespace ashford
