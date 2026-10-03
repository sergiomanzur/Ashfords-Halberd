#pragma once
#include "scenario.hpp"
namespace ashford::detail {
struct MotionCorrection {
  size_t actor;
  Vec3 position;
};
struct MotionFixResult {
  bool skip = false;
  std::optional<MotionCorrection> correction;
};
// Extracted source switch. Motion2 selects by80000 and ignores mtn_md mirror.
MotionFixResult evaluate_zombie_motion_fix(const ScenarioState &state,
                                           size_t work, int32_t type,
                                           bool respect_mirror = true);
} // namespace ashford::detail
