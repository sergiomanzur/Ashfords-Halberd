#pragma once
#include "scenario.hpp"
namespace ashford::detail {
// Shared bhSetMotion channel/counter application. Caller owns rollback and
// must retain a prepared full owner pose/cache for per-frame use.
void apply_zombie_motion_channels(ScenarioState &state, size_t work);
} // namespace ashford::detail
