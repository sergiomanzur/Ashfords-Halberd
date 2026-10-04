#pragma once
#include "scene.hpp"
#include <bitset>
namespace ashford {
// Original CPU palbuf has4096 colors. Normal drawing samples its first1024.
// Retain GS-range alpha here; RGBA expansion happens at native resolution.
struct NativeIndexedTexture {
  uint32_t width = 0, height = 0;
  uint8_t type = 0;
  Bytes indices;
};
struct NativeEffectTextureBinding {
  std::shared_ptr<const NativeIndexedTexture> image;
  std::shared_ptr<const Texture> direct;
  uint32_t global_index = 0; // Aliases retain the preceding source identity.
  uint8_t bank = 0;
  bool shared_bank = false; // Ordinary njLoadTexture entries use bank=-1.
};
struct NativeEffectTextureLibrary {
  std::array<std::array<uint8_t, 4>, 4096> palette{};
  std::bitset<4096> initialized;
  uint16_t next_bank = 32; // system.c initializes sys->ef_pbnk to32.
  size_t emitted_pixels = 0;
  std::vector<NativeEffectTextureBinding> textures;
};
struct NativeEffectTextureRange {
  size_t first = 0, count = 0;
};
// Atomic pack import. Nonzero TEXflag writes colors without adding a binding.
// Keep one library across all room effect families, then resolve for drawing.
NativeEffectTextureRange
load_effect_texture_pack(const Bytes &data, size_t offset,
                         NativeEffectTextureLibrary &library);
Texture resolve_effect_texture(const NativeEffectTextureLibrary &library,
                               size_t texture);
} // namespace ashford
