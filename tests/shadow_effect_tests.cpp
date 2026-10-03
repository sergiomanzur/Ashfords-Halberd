#include "effects.hpp"
#include "scenario.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
void ck(bool value, const char *why) {
  if (!value)
    throw std::runtime_error(why);
}
template <class F> void bad(F action) {
  bool failed = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Expected invalid shadow rejection");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.enemy_slots = {7};
  auto &e = s.enemies[7];
  e.flags = 1;
  e.model_work.emplace();
  e.model_work->joints.resize(4);
  for (auto &j : e.model_work->joints) {
    j.matrix[3] = 10;
    j.matrix[7] = 50;
    j.matrix[11] = 20;
  }
  e.model_work->joints[2].matrix[3] = 0;
  e.model_work->joints[2].matrix[11] = 17;
  e.model_work->joints[3].matrix[3] = 20;
  e.model_work->joints[3].matrix[11] = 23;
  s.collision.emplace();
  CollisionRecord wall;
  wall.flags = 1;
  wall.type = 0;
  wall.position = {-100, 0, -100};
  wall.size = {200, 5, 200};
  s.collision->walls.push_back(wall);
  s.flags[6][0] = 8;
  return s;
}
void creation() {
  auto s = fixture();
  auto prior = std::make_shared<NativeEffectPool>();
  prior->works[0].flags = 2;
  prior->works[1].flags = 3;
  prior->works[2].flags = 8;
  prior->works[2].state_flags = 0xffffffff;
  prior->scratch.id = 370;
  s.effect_pool = prior;
  auto slot = create_enemy_shadow(s, 7, EnemyShadow{{}, 1, {4.5f, 4, 3.5f}});
  ck(slot == 2 && s.effect_pool != prior && prior->works[2].flags == 8,
     "Shadow birth first free flags3 and immutable retention");
  const auto &w = s.effect_pool->works[2];
  ck(w.flags == 0x240081 && w.state_flags == 0 && w.id == 1 && w.type == 1 &&
         w.texture_id == -1 && w.linked_parent == 7 && w.linked_joint == 1 &&
         w.shadow_joints[0] == -1 && w.scale.x == 4.5f &&
         w.base_scale.z == 3.5f && w.position.x == 0 && w.modes[0] == 0 &&
         s.effect_pool->scratch.id == 370,
     "Original bhSetShadow fields and null joint table");
  slot = create_enemy_shadow(s, 7, EnemyShadow{{2, 3, -1}, 1, {8, 4, 4}});
  ck(slot == 3 && s.effect_pool->works[3].type == 0 &&
         s.effect_pool->works[3].shadow_joints[0] == 2 &&
         s.effect_pool->works[3].shadow_joints[2] == -1,
     "Shadow birth joint table");
  for (uint8_t variant : {0, 1, 2, 3, 4, 5}) {
    auto current = fixture();
    run_scenario(Bytes{47, 0, variant, 0, 0}, current);
    ck(current.enemies[7].flags == 0x801, "Shadow opcode authored work map");
    if (variant == 5) {
      ck(!current.effect_pool,
         "Unsupported source variant has no shadow birth");
      continue;
    }
    const auto &e = current.effect_pool->works[0];
    ck(e.scale.x == (variant == 4 ? 6.5f : 4.5f) && e.scale.y == 4 &&
           e.scale.z == (variant == 4 ? 4.5f : 3.5f),
       "Shadow opcode original variant dimensions");
    const auto retained = current.effect_pool;
    run_scenario(Bytes{47, 0, 4, 0, 0}, current);
    ck(current.effect_pool == retained, "Source800 duplicate gate");
  }
  auto invalid = fixture();
  bad([&] { run_scenario(Bytes{47, 0, 0}, invalid); });
  bad([&] { request_enemy_shadow(invalid, 1, 0); });
  ck(invalid.enemies[7].flags == 1 && !invalid.effect_pool,
     "Invalid opcode extent/work must preserve state");
  auto full = std::make_shared<NativeEffectPool>();
  for (auto &e : full->works)
    e.flags = 3;
  s.effect_pool = full;
  ck(!create_enemy_shadow(s, 7, EnemyShadow{{}, 1, {1, 2, 3}}) &&
         s.effect_pool == full,
     "Source exhausted shadow allocation returns no slot");
  request_enemy_shadow(s, 0, 0);
  ck(s.enemies[7].flags == 0x801 && s.effect_pool == full,
     "Source800 remains set on pool exhaustion");
}
void control() {
  auto s = fixture();
  create_enemy_shadow(s, 7, EnemyShadow{{2, 3, -1}, 1, {8, 4, 4}});
  auto initial = s.effect_pool;
  tick_shadow_effects(s);
  auto first = s.effect_pool;
  ck(first != initial && initial->works[0].modes[0] == 0 &&
         first->works[0].modes[0] == 1 && first->works[0].flags == 0x8240081 &&
         first->works[0].position.x == 10 && first->works[0].position.y == 50 &&
         first->works[0].position.z == 20 && first->works[0].scale.x == 8,
     "First source shadow callback tracks link and only initializes modifier "
     "mode");
  tick_shadow_effects(s);
  const auto &second = s.effect_pool->works[0];
  ck(second.position.y == 5 && second.scale.x == 16 &&
         std::abs(second.scale.z - 4.8f) < 0.00001f &&
         s.effect_pool->ground_wall == 0,
     "Source ground and clamped joint footprint");
  s.enemies[7].angles[1] = 16384;
  tick_shadow_effects(s);
  ck(s.effect_pool->works[0].angles[1] == 16384 &&
         s.effect_pool->works[0].scale.x == 8 &&
         s.effect_pool->works[0].scale.z == 8,
     "Yaw transforms the source footprint into local space");
  s.shadow_flags = 0;
  tick_shadow_effects(s);
  ck(s.effect_pool->works[0].flags & 0x1000000,
     "Shadow flags disable non40000000 owner");
  s.shadow_flags = 3;
  s.enemies[7].state_flags = 0x1000000;
  tick_shadow_effects(s);
  ck(!(s.effect_pool->works[0].flags & 0x1000000),
     "Source pt flag1 overrides hidden parent");
  s.shadow_flags = 2;
  tick_shadow_effects(s);
  ck(s.effect_pool->works[0].flags & 0x1000000, "Source hidden parent gate");
  auto hidden = std::make_shared<NativeEffectPool>(*s.effect_pool);
  hidden->works[0].state_flags = 0x1000000;
  s.effect_pool = hidden;
  s.enemies[7].model_work.reset();
  tick_shadow_effects(s);
  ck(s.effect_pool == hidden,
     "Hidden source effect must not touch invalid parent cache");
  auto frozen = fixture();
  create_enemy_shadow(frozen, 7, EnemyShadow{{}, 1, {1, 2, 3}});
  frozen.flags[6][0] = 0;
  const auto old = frozen.effect_pool;
  tick_shadow_effects(frozen);
  ck(frozen.effect_pool == old, "Source subsystem gate must freeze shadow");
  frozen.effect_flags = 1;
  tick_shadow_effects(frozen);
  ck(frozen.effect_pool->works[0].modes[0] == 1,
     "Effect flag1 enables shadow callback");
  auto unsupported = fixture();
  create_enemy_shadow(unsupported, 7, EnemyShadow{{}, 1, {1, 2, 3}});
  tick_shadow_effects(unsupported);
  unsupported.enemies[7].state_flags = 0x2000;
  const auto unchanged = unsupported.effect_pool;
  bad([&] { tick_shadow_effects(unsupported); });
  ck(unsupported.effect_pool == unchanged,
     "Missing source ground cache rejects without advancing pool");
  auto batch = fixture();
  create_enemy_shadow(batch, 7, EnemyShadow{{}, 1, {1, 2, 3}});
  create_enemy_shadow(batch, 7, EnemyShadow{{}, 1, {4, 5, 6}});
  tick_shadow_effects(batch);
  auto malformed = std::make_shared<NativeEffectPool>(*batch.effect_pool);
  malformed->works[1].linked_joint = 999;
  batch.effect_pool = malformed;
  bad([&] { tick_shadow_effects(batch); });
  ck(batch.effect_pool == malformed &&
         batch.effect_pool->works[0].position.y == 50,
     "Late invalid shadow must preserve earlier owner and ground side effects");
  auto inactive = fixture();
  create_enemy_shadow(inactive, 7, EnemyShadow{{}, 1, {1, 2, 3}});
  tick_shadow_effects(inactive);
  inactive.enemies[7].flags = 0;
  tick_shadow_effects(inactive);
  ck(inactive.effect_pool->works[0].flags == 0 &&
         inactive.effect_pool->ground_wall == 0,
     "Inactive parent releases shadow after original ground side effect");
  auto aliased = fixture();
  aliased.enemies[7].pose_owner = 0;
  aliased.enemies[0].model_work = aliased.enemies[7].model_work;
  aliased.enemies[7].model_work.reset();
  create_enemy_shadow(aliased, 7, EnemyShadow{{}, 1, {1, 2, 3}});
  tick_shadow_effects(aliased);
  ck(aliased.effect_pool->works[0].position.x == 10,
     "Linked zombie body uses retained shared model owner");
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Missing mode");
    if (std::string(argv[1]) == "creation")
      creation();
    else
      control();
    std::cout << "Native shadow checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
