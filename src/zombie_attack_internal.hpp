#pragma once
#include "scenario.hpp"
namespace ashford::detail {
// EatCheck against a staged yaw, retaining the original work's cache owner.
bool zombie_capture_yaw(const ScenarioState &state, size_t work,
                        const ScenarioEntity &player, int32_t yaw, int32_t cone,
                        float distance, int mode);
} // namespace ashford::detail
