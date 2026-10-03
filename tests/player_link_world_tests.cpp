#include "effects.hpp"
#include "ninja_matrix.hpp"
#include "zombie.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
bool near(float a, float b) { return std::abs(a - b) < .00002f; }
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Malformed PlayerLink dependency accepted");
}
ScenarioState fixture() {
  ScenarioState s;
  s.collision.emplace();
  s.collision->height = 20;
  s.enemies.resize(128);
  auto &e = s.enemies[0];
  e.flags = 1;
  e.id = 1;
  e.position = {10, 5, 20};
  e.zombie_work.emplace();
  e.zombie_work->action_anchor = {1, 2, 3};
  return s;
}
ScenarioEntity player() {
  ScenarioEntity p;
  p.position = {-50, 7, -60};
  p.flags = 0x1101;
  p.geometry.radius = 1;
  p.geometry.height = 10;
  p.motion_number = 401;
  p.model_matrix = detail::ninja_store(detail::ninja_identity());
  (*p.model_matrix)[3] = -99;
  p.yaw_correction = 16384;
  return p;
}
void transforms() {
  auto s = fixture();
  auto p = player();
  p.frame = 65535;
  p.model_work.emplace();
  p.model_work->base_matrix[3] = std::numeric_limits<double>::quiet_NaN();
  link_zombie_player(s, 0, p);
  ck(p.position.x == 11 && p.position.y == 7 && p.position.z == 23 &&
         p.angles[1] == 0,
     "PlayerLink anchor/retained Y");
  ck(near(float((*p.model_matrix)[3]), 11) &&
         near(float((*p.model_matrix)[7]), 7) &&
         near(float((*p.model_matrix)[11]), 22.999956f),
     "PlayerLink authored offset/matrix translation");
  ck(near(float((*p.model_matrix)[2]), 1) &&
         near(float((*p.model_matrix)[8]), -1),
     "PlayerLink matrix must use yaw correction");
  ck(s.enemies[0].position.x == 10 && s.enemies[0].position.y == 5 &&
         s.enemies[0].position.z == 20 && p.flags == 0x1101,
     "No-hit correction/work flags");
  ck(std::isnan(p.model_work->base_matrix[3]),
     "PlayerLink consumed/mutated ML_WORK instead of per-work matrix");
  for (uint8_t character = 0; character < 4; ++character)
    for (uint32_t clip : {1u, 2u, 4u, 5u}) {
      s = fixture();
      s.player_id = character;
      p = player();
      s.enemies[0].angles = {16384, 16384, 16384};
      s.enemies[0].grab_yaw_offset = -32768;
      p.angles = {16384, 0, 16384};
      p.yaw_correction = 12345;
      p.motion_number = (character == 0   ? 400
                         : character == 2 ? 450
                                          : 425) +
                        clip;
      p.frame = 25 * 65536 + 32768;
      auto enemy = detail::ninja_identity();
      detail::ninja_translate(enemy, s.enemies[0].position);
      detail::ninja_rotate(enemy, s.enemies[0].angles);
      const auto anchor = detail::ninja_point(enemy, {1, 2, 3});
      const uint16_t yaw = uint16_t(16384 - 32768);
      auto actual = detail::ninja_identity();
      detail::ninja_translate(actual, {anchor.x, 7, anchor.z});
      detail::ninja_rotate(actual, {16384, yaw, 16384});
      const auto proposed = detail::ninja_point(
          actual, zombie_player_link_offset(character, p.motion_number, 25));
      auto expected = detail::ninja_identity();
      detail::ninja_translate(expected, proposed);
      detail::ninja_rotate(expected, {16384, 12345, 16384});
      link_zombie_player(s, 0, p);
      ck(p.angles[1] == yaw && p.position.x == anchor.x && p.position.y == 7 &&
             p.position.z == anchor.z &&
             *p.model_matrix == detail::ninja_store(expected),
         "Four characters/clips fractional frame and XYZ rotations");
    }
  s = fixture();
  s.player_id = 255;
  p = player();
  p.flags = 0x101;
  p.motion_number = UINT32_MAX;
  p.frame = INT32_MIN;
  const auto old = *p.model_matrix;
  link_zombie_player(s, 0, p);
  ck(*p.model_matrix == old && p.position.x == 11 && p.position.z == 23,
     "Non-table PlayerLink consumed unused character/clip/frame or rebuilt "
     "mtx");
}
void collision_and_rollback() {
  auto s = fixture();
  auto p = player();
  p.flags = 0x101;
  s.enemies[0].position = {0, 0, 0};
  s.enemies[0].zombie_work->action_anchor = {2, 0, 2};
  CollisionRecord w;
  w.flags = 1;
  w.size = {4, 10, 4};
  s.collision->walls = {w};
  (*p.model_matrix)[3] = -5;
  (*p.model_matrix)[11] = 2;
  link_zombie_player(s, 0, p);
  // Prior X=-5 and Z=2 select lower-left: source prior face gaps4>3
  // select Z=5 (delta+3), even though the prior point lies left of the box.
  ck(p.position.x == 2 && p.position.y == 7 && p.position.z == 2 &&
         s.enemies[0].position.x == 0 && s.enemies[0].position.y == 0 &&
         s.enemies[0].position.z == 3,
     "Wall2 correction must move enemy not player anchor");
  ck((p.state_flags & 1) && p.flags == 0x101 && (*p.model_matrix)[3] == -5,
     "Collision state/matrix retention");
  for (int fault = 0; fault < 4; ++fault) {
    s = fixture();
    p = player();
    if (fault == 0)
      p.model_matrix.reset();
    if (fault == 1)
      p.motion_number = 403;
    if (fault == 2)
      p.frame = 60 * 65536;
    if (fault == 3) {
      w.position.x = std::numeric_limits<float>::quiet_NaN();
      s.collision->walls = {w};
    }
    const auto oldpos = p.position;
    const auto oldangles = p.angles;
    const auto oldmatrix = p.model_matrix;
    bad([&] { link_zombie_player(s, 0, p); });
    ck(p.position.x == oldpos.x && p.position.y == oldpos.y &&
           p.position.z == oldpos.z && p.angles == oldangles &&
           p.model_matrix == oldmatrix && p.state_flags == 0 &&
           !s.effect_pool && s.enemies[0].position.x == 10,
       "PlayerLink late failure rollback");
  }
  s = fixture();
  bad([&] { link_zombie_player(s, 0, s.enemies[0]); });
  p = player();
  bad([&] { link_zombie_player(s, 128, p); });
}
int main() {
  try {
    transforms();
    collision_and_rollback();
    std::cout << "Complete native PlayerLink world/matrix/collision "
                 "transaction passes\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
