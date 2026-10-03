#pragma once
#include "scenario.hpp"
namespace ashford::detail {
// Small scalar staging for original ChgMtn; no geometry/pose copies per tick.
struct MotionState {
  uint32_t motion_number, motion_step, motion_mode, flags;
  int32_t frame, blend_count, blend_rate;
};
inline MotionState snapshot(const ScenarioEntity &enemy) {
  return {enemy.motion_number, enemy.motion_step, enemy.motion_mode,
          enemy.flags,         enemy.frame,       enemy.blend_count,
          enemy.blend_rate};
}
inline void commit(ScenarioEntity &enemy, const MotionState &motion) {
  enemy.motion_number = motion.motion_number;
  enemy.motion_step = motion.motion_step;
  enemy.motion_mode = motion.motion_mode;
  enemy.flags = motion.flags;
  enemy.frame = motion.frame;
  enemy.blend_count = motion.blend_count;
  enemy.blend_rate = motion.blend_rate;
}
template <class T>
int change(T &enemy, uint32_t number, int32_t frame, int32_t rate) {
  enemy.motion_step = 65536;
  if (enemy.motion_number == number)
    return -1;
  enemy.motion_number = number;
  enemy.frame = frame;
  enemy.blend_count = rate;
  enemy.blend_rate = 0;
  enemy.motion_mode = 0x20;
  enemy.flags &= ~0x2040000u;
  return 0;
}
} // namespace ashford::detail
