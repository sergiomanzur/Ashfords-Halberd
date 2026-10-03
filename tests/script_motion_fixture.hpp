#pragma once
#include "effects.hpp"
#include "scenario.hpp"
#include <algorithm>
#include <stdexcept>
namespace script_fixture {
using namespace ashford;
inline void ck(bool value, const char *why) {
  if (!value)
    throw std::runtime_error(why);
}
template <class F> void bad(F action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Malformed scripted motion was accepted");
}
inline void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
inline ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.enemy_slots = {2};
  ActorModel model;
  model.model.resize(16 + 18 * 52);
  model.model[0] = 'M';
  model.model[1] = 'D';
  model.model[2] = 'L';
  model.model[3] = 128;
  model.model[4] = 16;
  model.model[6] = 18;
  put(model.model, 8, 0xffffffff);
  for (size_t j = 0; j < 18; ++j) {
    const auto p = 16 + j * 52;
    put(model.model, p, 6);
    put(model.model, p + 4, 0xffffffff);
    if (j)
      put(model.model, p + 12, 0x3f800000);
    put(model.model, p + 44, j == 17 ? 0xffffffff : uint32_t((j + 1) * 52));
    put(model.model, p + 48, 0xffffffff);
  }
  auto &r = s.enemies[2];
  r.flags = 1;
  r.id = 1;
  r.position = {100, 200, 300};
  r.model = std::make_shared<ActorModel>(std::move(model));
  r.joint_pose = actor_bind_pose(*r.model);
  ActorPlacement p;
  p.position = r.position;
  r.model_work = calculate_actor_model_work(*r.model, r.joint_pose, {}, p);
  r.model_matrix = r.model_work->base_matrix;
  r.zombie_work.emplace();
  r.zombie_work->body = 3;
  r.zombie_flags = 1;
  r.base_motions = std::make_shared<MotionBank>(512);
  r.motions = r.base_motions;
  auto &b = s.enemies[3];
  b.flags = 0x81;
  b.id = 1;
  b.pose_owner = 2;
  b.linked_parent = 2;
  b.linked_joint = 1;
  b.model = r.model;
  b.position = r.position;
  b.zombie_work.emplace();
  b.zombie_flags = 0x80000000;
  b.base_motions = r.base_motions;
  b.motions = r.motions;
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t n : {4u, 5u}) {
    Motion m;
    m.flags = 0x10;
    m.frames = 3;
    m.tracks.resize(n == 4 ? 18 : 10);
    for (auto &t : m.tracks)
      t.angles.assign(3, {});
    m.tracks[0].positions =
        n == 4 ? std::vector<Vec3>{{2, 0, 0}, {4, 0, 0}, {6, 0, 0}}
               : std::vector<Vec3>{{0, 4, 0}, {0, 6, 0}, {0, 8, 0}};
    (*bank)[n] = std::move(m);
  }
  s.room_motions = bank;
  return s;
}
inline void event(ScenarioState &s, const Bytes &code) {
  Bytes data(1214 + code.size());
  put(data, 0, 0x41200000);
  put(data, 16, 128);
  put(data, 28, 1200);
  put(data, 1200, 12);
  put(data, 1204, 13);
  put(data, 1208, 14);
  std::copy(code.begin(), code.end(), data.begin() + 1214);
  s.scenario = std::make_shared<RoomScenario>(decode_room_scenario(data));
  s.flags[6][0] = 0x10;
  start_scenario_event(s, 0, 0);
  tick_scenario_tasks(s);
}
} // namespace script_fixture
