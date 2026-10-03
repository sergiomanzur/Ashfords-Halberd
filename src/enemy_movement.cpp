#include "enemy_movement.hpp"
#include "enemy_targeting.hpp"
#include "ninja_matrix.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp zonzon.c and ps2_NaMatrix.c.
namespace ashford {
namespace {
int32_t word(uint32_t value) {
  return value < 0x80000000u ? int32_t(value)
                             : int32_t(int64_t(value) - 0x100000000ll);
}
float finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite enemy motion displacement");
  return value;
}
Vec3 root_delta(const ScenarioEntity &e, int32_t frame, int mode) {
  if (!e.motions || e.motion_number >= e.motions->size() ||
      !(*e.motions)[e.motion_number])
    throw std::runtime_error("Missing enemy displacement motion");
  const auto &m = *(*e.motions)[e.motion_number];
  if (frame < 0 || uint32_t(frame) >= m.frames || m.tracks.empty() ||
      size_t(frame) >= m.tracks[0].positions.size())
    throw std::runtime_error("Missing enemy root position key");
  const auto &keys = m.tracks[0].positions;
  auto delta = keys[frame];
  if (frame) {
    delta.x -= keys[frame - 1].x;
    delta.y -= keys[frame - 1].y;
    delta.z -= keys[frame - 1].z;
  }
  if (!mode)
    delta.y = 0;
  finite(delta.x);
  finite(delta.y);
  finite(delta.z);
  return delta;
}
} // namespace
int32_t turn_enemy_yaw(const ScenarioEntity &enemy, Vec3 target, int32_t step) {
  if (enemy.flags & 0x80)
    return enemy.angles[1];
  int32_t angle = enemy_direction(enemy.position, target);
  if (step < 0) {
    step = word(0u - uint32_t(step));
    angle = uint16_t(uint32_t(angle) + 32768u);
  }
  const auto twice = word(uint32_t(step) + uint32_t(step));
  const int32_t rotation =
      uint16_t(uint32_t(step) + uint32_t(angle) - uint32_t(enemy.angles[1]));
  if (rotation < twice)
    return angle;
  auto yaw = uint32_t(enemy.angles[1]) - uint32_t(step);
  if (rotation <= 32768)
    yaw += uint32_t(twice);
  return word(yaw);
}
int32_t enemy_turn_step(const ScenarioEntity &enemy, Vec3 target,
                        int32_t step) {
  if (enemy.flags & 0x80)
    return 0;
  const int32_t rotation = uint16_t(
      uint32_t(step) + uint32_t(enemy_direction(enemy.position, target)) -
      uint32_t(enemy.angles[1]));
  if (rotation < word(uint32_t(step) + uint32_t(step)))
    return 0;
  return rotation < 32769 ? step : word(0u - uint32_t(step));
}
void translate_enemy_motion(ScenarioEntity &enemy, int32_t frame, int mode) {
  if (!enemy.motion_step && enemy.frame / 65536 == frame)
    return;
  const auto delta = root_delta(enemy, frame, mode);
  // Source rotates matrix columns Z,Y,X, retaining float operations and
  // Ninja table rounding, then multiplies the point X,Y,Z and translation.
  auto matrix = detail::ninja_identity();
  detail::ninja_translate(matrix, enemy.position);
  detail::ninja_rotate(matrix, enemy.angles);
  auto next = detail::ninja_point(matrix, delta);
  const float quantized = finite(100.f * (0.001f + next.y));
  if (double(quantized) < -2147483648. || double(quantized) >= 2147483648.)
    throw std::runtime_error(
        "Enemy displacement Y outside source integer range");
  next.y = int32_t(quantized) / 100.f;
  enemy.position = next;
}
void measure_enemy_motion_speed(ScenarioEntity &enemy, int32_t frame,
                                int mode) {
  if (!enemy.motion_step && enemy.frame / 65536 == frame)
    return;
  const auto delta = root_delta(enemy, frame, mode);
  const float squared =
      finite((delta.x * delta.x + delta.y * delta.y) + delta.z * delta.z);
  enemy.speed = std::sqrt(squared);
}
} // namespace ashford
