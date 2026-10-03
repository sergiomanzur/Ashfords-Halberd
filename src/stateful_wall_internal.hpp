#pragma once
#include "stateful_wall.hpp"
#include <functional>
namespace ashford::detail {
// PlayerLink validates its outer world correction before this transaction
// commits. The callback may compute staged scalars, but must not mutate state.
StatefulWallResult run_stateful_wall(
    ScenarioState &, ScenarioEntity &work, ScenarioEntity &player,
    Vec3 proposed, Vec3 previous, float radius, float height, bool wall2,
    const std::function<void(const StatefulWallResult &)> &before_commit = {});
} // namespace ashford::detail
