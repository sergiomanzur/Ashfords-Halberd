#include "effects.hpp"
#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
namespace {
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < .00001f; }
template <class F> void bad(F fn) {
  bool rejected = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Invalid consumed head-effect context accepted");
}
ScenarioState fixture() {
  ScenarioState s;
  s.random_seed = 1;
  s.enemies.resize(128);
  auto &e = s.enemies[0];
  e.flags = 1;
  e.id = 1;
  e.angles[1] = 16384;
  e.damage_joint = 5;
  e.damage_velocity = {7, 8, 9};
  e.model_work.emplace();
  e.model_work->joints.resize(11);
  e.model_work->joints[10].matrix = {0,  0, 1, 10, 0, 1, 0, 20,
                                     -1, 0, 0, 30, 0, 0, 0, 1};
  auto pool = std::make_shared<NativeEffectPool>();
  pool->scratch.model_version = 258;
  pool->scratch.floor = 258;
  pool->scratch.angles = {-123, 234};
  pool->scratch.position = {100, 200, 300};
  s.effect_pool = pool;
  return s;
}
size_t active(const NativeEffectPool &pool) {
  size_t count = 0;
  for (const auto &w : pool.works)
    count += bool(w.flags & 1);
  return count;
}
int texture_delta(uint8_t model) {
  switch (model) {
  case 2:
  case 3:
  case 9:
  case 10:
  case 32:
  case 33:
  case 39:
  case 43:
    return 0;
  case 4:
  case 21:
  case 34:
    return 1;
  case 7:
  case 8:
  case 31:
  case 37:
  case 38:
  case 42:
    return 2;
  case 0:
  case 1:
  case 5:
  case 6:
  case 22:
  case 35:
  case 36:
    return 3;
  case 40:
    return 4;
  default:
    return 5;
  }
}
} // namespace
int main() {
  try {
    auto s = fixture();
    const auto prior = s.effect_pool;
    const auto cache = s.enemies[0].model_work->joints[10].matrix;
    tick_zombie_neck_blood_effect(s, 0, 0);
    ck(active(*s.effect_pool) == 8 && active(*prior) == 0 && s.random_seed == 1,
       "Neck blood requires eight births, immutable pool and no RNG");
    // ps2_NaMath.c reflects about16383, so source quadrants are asymmetric.
    const Vec3 blood_directions[8] = {{-.392791122f, .831469655f, -.392753482f},
                                      {-.653255522f, .382683456f, -.653192937f},
                                      {-.392753482f, .831469655f, .392791122f},
                                      {-.653192937f, .382683456f, .653255522f},
                                      {.392791122f, .831469655f, .392753482f},
                                      {.653255522f, .382683456f, .653192937f},
                                      {.392753482f, .831469655f, -.392791122f},
                                      {.653192937f, .382683456f, -.653255522f}};
    for (size_t i = 0; i < 8; ++i) {
      const auto &w = s.effect_pool->works[i];
      const bool upper = !(i & 1);
      ck(w.flags == 0x81 && w.id == 370 && w.state_flags == 0x20 &&
             w.type == (upper ? 1 : 3) && w.control0 == int(i / 2) &&
             w.control1 == (upper ? 6 : 3) && w.linked_parent == 0 &&
             w.linked_joint == 10 && w.texture_owner == 0 &&
             w.texture_id == 9 && !w.texture_slot && !w.particle_storage &&
             w.offset.x == 0 && w.offset.y == 0 && w.offset.z == 0 &&
             w.position.x == 0 && w.position.y == 0 && w.position.z == 0 &&
             w.scale.x == 3.5f && w.scale.y == 3.5f && w.scale.z == 0 &&
             w.base_scale.x == 3.5f && w.model_version == 2 && w.floor == 2 &&
             w.angles == std::array<int32_t, 3>{-123, 234, 0} &&
             near(w.direction.x, blood_directions[i].x) &&
             near(w.direction.z, blood_directions[i].z) &&
             near(w.direction.y, blood_directions[i].y),
         "Source blood order/direction/link/counters/retained scratch changed");
    }
    ck(s.effect_pool->scratch.id == 370 && s.effect_pool->scratch.type == 3 &&
           s.effect_pool->scratch.model_version == 258 &&
           s.effect_pool->scratch.floor == 258 &&
           s.enemies[0].model_work->joints[10].matrix == cache &&
           s.enemies[0].damage_joint == 5 &&
           s.enemies[0].damage_velocity.x == 7,
       "Blood must preserve unused template and enemy cache/damage fields");
    ScenarioState unused;
    tick_zombie_neck_blood_effect(unused, 999, -1);
    tick_zombie_neck_blood_effect(unused, 999, 1);
    ck(!unused.effect_pool, "Nonzero neck type must ignore all context");

    s = fixture();
    tick_zombie_head_fragment_effect(s, 0, 123); // Source ignores type.
    ck(active(*s.effect_pool) == 4 && s.random_seed == 3295386429u,
       "Fragments require four births and four source LCG calls");
    const int32_t rolls[4] = {1103527590, 377401575, 662824084, 1147902781};
    const Vec3 fragment_directions[4] = {
        {-.653192937f, .382683456f, .653255522f},
        {.653255522f, .382683456f, .653192937f},
        {.653192937f, .382683456f, -.653255522f},
        {-.653255522f, .382683456f, -.653192937f}};
    for (size_t i = 0; i < 4; ++i) {
      const auto &w = s.effect_pool->works[i];
      const float scale = 2.f + float(-rolls[i]) / -2.1474836E9f;
      ck(w.flags == 1 && w.id == 362 && w.type == 0 && w.state_flags == 0x20 &&
             !w.linked_parent && w.linked_joint == 0 && w.texture_owner == 0 &&
             w.texture_id == 8 && w.position.x == 9 && w.position.y == 20.5f &&
             w.position.z == 30 && w.scale.x == scale && w.scale.y == scale &&
             w.scale.z == 0 && w.base_scale.x == scale && w.control0 == 0 &&
             w.control1 == 0 && w.model_version == 2 && w.floor == 2 &&
             w.angles == std::array<int32_t, 3>{-123, 234, 0} &&
             near(w.direction.x, fragment_directions[i].x) &&
             near(w.direction.z, fragment_directions[i].z) &&
             near(w.direction.y, fragment_directions[i].y),
         "Source fragment transformed point/scale/yaw/template changed");
    }
    ck(s.enemies[0].model_work->joints[10].matrix == cache &&
           s.enemies[0].damage_velocity.x == 7,
       "Fragment birth must not change original cache/damage velocity");
    for (int model = 0; model < 256; ++model) {
      s = fixture();
      s.enemies[0].model_version = uint8_t(model);
      tick_zombie_neck_blood_effect(s, 0, 0);
      tick_zombie_head_fragment_effect(s, 0, 0);
      ck(s.effect_pool->works[0].texture_id ==
                 6 + texture_delta(uint8_t(model)) &&
             s.effect_pool->works[8].texture_id ==
                 5 + texture_delta(uint8_t(model)),
         "Full source texture-version remap changed");
    }
    s = fixture();
    s.enemies[1] = s.enemies[0];
    s.enemies[1].pose_owner = 0;
    s.enemies[1].model_work.reset();
    tick_zombie_neck_blood_effect(s, 1, 0);
    tick_zombie_head_fragment_effect(s, 1, 0);
    ck(s.effect_pool->works[0].linked_parent == 1 &&
           s.effect_pool->works[0].texture_owner == 0 &&
           s.effect_pool->works[8].texture_owner == 0 &&
           s.effect_pool->works[8].position.x == 9,
       "Shared model texture owner must differ from blood linked parent");
    s = fixture();
    auto full = std::make_shared<NativeEffectPool>();
    for (auto &w : full->works) {
      w.flags = 1;
      w.id = 77;
    }
    s.effect_pool = full;
    s.enemies[0].model_work.reset();
    tick_zombie_neck_blood_effect(s, 0, 0);
    ck(active(*s.effect_pool) == 512 && s.effect_pool->scratch.id == 370 &&
           s.effect_pool->works[0].id == 77 && s.random_seed == 1,
       "Full blood pool must ignore unused cache and retain scratch");
    s = fixture();
    s.effect_pool = full;
    tick_zombie_head_fragment_effect(s, 0, 0);
    ck(active(*s.effect_pool) == 512 && s.effect_pool->works[0].id == 77 &&
           s.effect_pool->scratch.id == 362 && s.random_seed == 3295386429u &&
           s.effect_pool->scratch.scale.x ==
               2.f + float(-rolls[3]) / -2.1474836E9f,
       "Full fragment pool must still consume four rolls and retain template");
    s = fixture();
    auto partial = std::make_shared<NativeEffectPool>(*full);
    partial->works[0].flags = 2; // Reserved slots remain occupied.
    partial->works[1].flags = partial->works[3].flags = 0;
    s.effect_pool = partial;
    tick_zombie_neck_blood_effect(s, 0, 0);
    ck(s.effect_pool->works[0].flags == 2 &&
           s.effect_pool->works[1].type == 1 &&
           s.effect_pool->works[3].type == 3 &&
           s.effect_pool->works[1].control0 == 0 &&
           s.effect_pool->works[3].control0 == 0 &&
           s.effect_pool->scratch.type == 3 && s.random_seed == 1,
       "Blood allocation must skip reservations, retain successful prefix");
    s = fixture();
    s.effect_pool = partial;
    tick_zombie_head_fragment_effect(s, 0, 0);
    ck(s.effect_pool->works[0].flags == 2 &&
           s.effect_pool->works[1].id == 362 &&
           s.effect_pool->works[3].id == 362 &&
           s.effect_pool->works[1].scale.x ==
               2.f + float(-rolls[0]) / -2.1474836E9f &&
           s.effect_pool->works[3].scale.x ==
               2.f + float(-rolls[1]) / -2.1474836E9f &&
           s.random_seed == 3295386429u,
       "Partial fragments must consume exhausted rolls after allocated prefix");
    s = fixture();
    s.enemies[0].model_work->joints.clear();
    tick_zombie_neck_blood_effect(s, 0, 0);
    ck(active(*s.effect_pool) == 8,
       "Linked blood birth must not consume deferred joint matrix");
    for (int invalid = 0; invalid < 5; ++invalid) {
      s = fixture();
      if (invalid == 0)
        s.enemies[0].model_work.reset();
      if (invalid == 1)
        s.enemies[0].pose_owner = 128;
      if (invalid == 2) {
        s.enemies[0].pose_owner = 1;
        s.enemies[1].flags = 1;
        s.enemies[1].pose_owner = 2;
      }
      if (invalid == 3)
        s.enemies[0].flags = 0;
      if (invalid == 4)
        s.enemies[0].id = 2;
      const auto before = s.effect_pool;
      bad([&] { tick_zombie_neck_blood_effect(s, 0, 0); });
      ck(s.effect_pool == before && active(*before) == 0 && s.random_seed == 1,
         "Invalid blood context must not commit staged births");
      bad([&] { tick_zombie_head_fragment_effect(s, 0, 0); });
      ck(s.effect_pool == before && s.random_seed == 1,
         "Invalid fragment context must not commit births or RNG");
    }
    for (int invalid = 0; invalid < 3; ++invalid) {
      s = fixture();
      if (invalid == 0)
        s.enemies[0].model_work->joints.resize(10);
      if (invalid == 1)
        s.enemies[0].model_work->joints[10].matrix[0] =
            std::numeric_limits<double>::quiet_NaN();
      if (invalid == 2)
        s.enemies[0].model_work->joints[10].matrix[15] = 2;
      const auto before = s.effect_pool;
      bad([&] { tick_zombie_head_fragment_effect(s, 0, 0); });
      ck(s.effect_pool == before && s.random_seed == 1,
         "Invalid consumed cached joint must leave pool and RNG intact");
    }
    std::cout
        << "Native source standing-grab neck blood/head fragments passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
