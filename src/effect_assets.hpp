#pragma once
#include "actor.hpp"
#include "texture.hpp"
namespace ashford {
struct ScenarioState;
struct NativeEffectAssets {
  NativeEffectTextureLibrary textures;
  std::array<size_t, 450> texture_first{}, family_texture_count{};
  std::array<uint8_t, 450> palette_bank{};
  std::bitset<450> texture_families;
  std::array<std::shared_ptr<const ActorModel>, 20> models{};
  size_t base_texture_count = 0;
};
// SYSTEM.AFS entry1, effect.c::bhInitEffect's four MDL records and base PVP.
std::shared_ptr<const NativeEffectAssets>
decode_global_effect_assets(const Bytes &system_entry);
// Source actor cursor is published by bind_room_props. Stages replacement,
// retains current palette memory/base images, and clears prior room families.
void bind_room_effect_assets(const Bytes &room, ScenarioState &state);
// Source ef_tn[tex_id]+ani_ct addressing; zero-animation base aliases are
// valid.
const NativeEffectTextureBinding &
effect_texture_binding(const NativeEffectAssets &assets, int32_t texture_id,
                       int32_t animation = 0);
Texture resolve_effect_asset(const NativeEffectAssets &assets,
                             int32_t texture_id, int32_t animation = 0);
} // namespace ashford
