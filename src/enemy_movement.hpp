#pragma once
#include "scenario.hpp"
namespace ashford {
// zonzon.c::ikou/ikou3; linked primary80 bypasses unused target data.
int32_t turn_enemy_yaw(const ScenarioEntity &enemy, Vec3 target, int32_t step);
int32_t enemy_turn_step(const ScenarioEntity &enemy, Vec3 target, int32_t step);
// Source GetTranslateMtn[2]. frm is the frame captured before bhSetMotion.
// The first updates world position; the second updates spd only. Neither
// advances motion nor zeroes the owner joint; SetMtn performs those actions.
void translate_enemy_motion(ScenarioEntity &enemy, int32_t frame, int mode);
void measure_enemy_motion_speed(ScenarioEntity &enemy, int32_t frame, int mode);
} // namespace ashford
