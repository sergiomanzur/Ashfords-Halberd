#pragma once
#include "scenario.hpp"
namespace ashford::detail {
struct ZombieAwarenessUpdate {
  uint8_t awareness, timeout;
  uint32_t flags;
};
// Pure scalar evaluation lets Brain00 validate subsequent route/stair work
// before committing awareness and pursuit together.
ZombieAwarenessUpdate evaluate_zombie_awareness(const ScenarioState &state,
                                                size_t work, Vec3 player);
} // namespace ashford::detail
