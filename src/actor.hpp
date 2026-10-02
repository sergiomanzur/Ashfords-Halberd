#pragma once
#include "motion.hpp"
namespace ashford {
struct SkinWeight {
  uint8_t joint = 0;
  float rotation_weight = 1;
  uint16_t auxiliary = 0;
};
struct SkinNode {
  uint32_t flags = 0, group = 0;
  std::vector<SkinWeight> vertices;
};
std::vector<SkinNode> decode_skin(const Bytes &data, size_t object_count);
struct ActorModel {
  Bytes model;
  Bytes skin;
  Bytes textures;
};
struct PlayerPackage {
  std::vector<ActorModel> models;
  std::vector<std::optional<Motion>> motions;
  Bytes auxiliary;
};
PlayerPackage decode_player_package(const Bytes &data,
                                    size_t key_limit = 2000000);
} // namespace ashford
