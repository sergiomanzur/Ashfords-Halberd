#pragma once
#include "enemy_navigation.hpp"
namespace ashford::detail {
struct StairPair {
  CollisionRecord *first;
  CollisionRecord *second;
};
// Validate both native indices before any caller modifies reservation flags.
StairPair stair_pair(ScenarioState &state, size_t index);
} // namespace ashford::detail
