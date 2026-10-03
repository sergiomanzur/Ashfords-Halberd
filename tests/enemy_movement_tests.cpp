#include "enemy_movement.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
template <class F> void bad(F f) {
  bool hit = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    hit = true;
  }
  ck(hit, "Invalid movement context did not reject");
}
bool near(float a, float b) { return std::fabs(a - b) < .0001f; }
ScenarioEntity fixture() {
  ScenarioEntity e;
  e.position = {10, -1.234f, 20};
  e.motion_number = 0;
  e.speed = 77;
  auto bank = std::make_shared<MotionBank>(1);
  Motion m;
  m.frames = 3;
  m.tracks.resize(1);
  m.tracks[0].positions = {{1, 2, -3}, {4, 6, -8}, {8, 12, -15}};
  (*bank)[0] = m;
  e.motions = bank;
  return e;
}
int main() {
  try {
    auto e = fixture();
    e.position = {0, 0, 0};
    e.angles[1] = 0;
    ck(turn_enemy_yaw(e, {-1, 0, 0}, 256) == 256, "ikou positive step failed");
    ck(turn_enemy_yaw(e, {1, 0, 0}, 256) == -256,
       "ikou must retain signed unmasked yaw");
    e.angles[1] = 16384 - 255;
    ck(turn_enemy_yaw(e, {-1, 0, 0}, 256) == 16384, "ikou strict snap failed");
    e.angles[1] = 16384 - 256;
    ck(enemy_turn_step(e, {-1, 0, 0}, 256) == 256, "ikou3 equality must step");
    e.angles[1] = 16384 - 255;
    ck(enemy_turn_step(e, {-1, 0, 0}, 256) == 0, "ikou3 snap failed");
    e.angles[1] = 0;
    ck(turn_enemy_yaw(e, {-1, 0, 0}, -256) == -256,
       "Negative ikou must reverse target");
    ck(enemy_turn_step(e, {1, 0, 0}, 256) == -256,
       "ikou3 negative step failed");
    e.flags = 0x80;
    e.angles[1] = -123;
    auto nan = std::numeric_limits<float>::quiet_NaN();
    ck(turn_enemy_yaw(e, {nan, 0, nan}, 256) == -123 &&
           enemy_turn_step(e, {nan, 0, nan}, 256) == 0,
       "Linked turn must bypass unused target");
    e = fixture();
    translate_enemy_motion(e, 0, 0);
    ck(near(e.position.x, 11) && near(e.position.y, -1.23f) &&
           near(e.position.z, 17) && e.frame == 0 && e.speed == 77,
       "Frame0 translation/Y quantization failed");
    e = fixture();
    translate_enemy_motion(e, 1, 1);
    ck(near(e.position.x, 13) && near(e.position.y, 2.76f) &&
           near(e.position.z, 15),
       "Root adjacent keys/full Y failed");
    e = fixture();
    e.angles = {16384, 16384, 16384};
    e.position = {0, 0, 0};
    translate_enemy_motion(e, 0, 1);
    ck(near(e.position.x, -3) && near(e.position.y, 2) &&
           near(e.position.z, -1),
       "Translation must apply ZYX matrix composition");
    e = fixture();
    measure_enemy_motion_speed(e, 1, 0);
    ck(near(e.speed, std::sqrt(34.f)) && e.position.x == 10 &&
           e.position.y == -1.234f,
       "Speed must flatten Y without moving actor");
    measure_enemy_motion_speed(e, 1, 1);
    ck(near(e.speed, std::sqrt(50.f)), "Speed nonzero mode Y failed");
    e = fixture();
    e.motion_step = 0;
    e.frame = -1;
    e.motions.reset();
    e.position.x = nan;
    translate_enemy_motion(e, 0, 0);
    measure_enemy_motion_speed(e, 0, 0);
    ck(e.speed == 77 && std::isnan(e.position.x),
       "Inactive signed frame division must bypass unused keys");
    e = fixture();
    auto pos = e.position;
    bad([&] { translate_enemy_motion(e, -1, 0); });
    ck(e.position.x == pos.x && e.position.y == pos.y && e.position.z == pos.z,
       "Invalid key partially moved actor");
    bad([&] { measure_enemy_motion_speed(e, 3, 0); });
    ck(e.speed == 77, "Invalid speed key partially mutated speed");
    auto bank = std::make_shared<MotionBank>(*e.motions);
    (*bank)[0]->tracks[0].positions[1].y = nan;
    e.motions = bank;
    measure_enemy_motion_speed(e, 1, 0);
    ck(near(e.speed, std::sqrt(34.f)), "Unused mode0 key Y must not reject");
    bad([&] { translate_enemy_motion(e, 1, 1); });
    ck(e.position.x == pos.x, "Invalid consumed Y did not roll back");
    e = fixture();
    e.position.y = std::numeric_limits<float>::max();
    bad([&] { translate_enemy_motion(e, 0, 0); });
    ck(e.position.y == std::numeric_limits<float>::max(),
       "Quantization overflow partially moved actor");
    std::cout
        << "Source turning, root-key displacement and scalar speed passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
