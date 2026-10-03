#pragma once
#include "scenario.hpp"
namespace ashford {
struct StatefulWallResult {
  Vec3 position{}, displacement{};
  size_t hits = 0;
  std::optional<size_t>
      wall; // Wall2 query uses original proposal after cleanup.
};
StatefulWallResult collide_stateful_walls_ex(ScenarioState &,
                                             ScenarioEntity &work,
                                             ScenarioEntity &player,
                                             Vec3 proposed, Vec3 previous,
                                             float radius, float height);
StatefulWallResult check_enemy_wall2(ScenarioState &, ScenarioEntity &work,
                                     ScenarioEntity &player, Vec3 proposed,
                                     Vec3 previous, float radius, float height);
} // namespace ashford
