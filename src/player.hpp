#pragma once
#include "scene.hpp"
namespace ashford {
using PlayerTranslationTables = std::vector<std::vector<float>>;
// Length-prefixed auxiliary records consumed by bhGetTransZ.
PlayerTranslationTables decode_player_translation_tables(const Bytes &data);
// Baseline tables 0-10 use frame+1. Later action tables use a different
// indexing convention and are deliberately rejected by this helper.
float player_translation_speed(const PlayerTranslationTables &tables,
                               size_t table, size_t frame);
// bhAddSpeed with the original quarter-wave njSin/njCos lookup convention.
Vec3 player_add_speed(Vec3 position, uint16_t yaw, float speed,
                      uint16_t relative_angle = 0);
} // namespace ashford
