#include "scenario_motion.hpp"
#include "script_motion_fixture.hpp"
#include <iostream>
using namespace ashford;
using namespace script_fixture;
ScenarioState linked() {
  auto s = fixture();
  s.enemies[2].joint_pose[0].position->x = 2;
  auto &aux = s.enemies[4];
  aux = s.enemies[3];
  aux.id = 31;
  aux.linked_joint = 0;
  aux.state_flags = 0x1000000; // CalcLinkModel includes hidden active work.
  auto pool = std::make_shared<NativeEffectPool>();
  auto &e = pool->works[7];
  e.flags = 0x81;
  e.linked_parent = 2;
  e.linked_joint = 1;
  s.effect_pool = pool;
  return s;
}
int main() {
  try {
    auto s = linked();
    const auto previous_pool = s.effect_pool;
    calculate_scenario_models(s, 1, 2, true, true);
    ck(s.enemies[2].model_matrix->at(3) == 100 &&
           s.enemies[3].model_matrix->at(3) == 102 &&
           s.enemies[3].model_matrix->at(7) == 201 &&
           s.enemies[4].model_matrix->at(3) == 104 &&
           s.enemies[2].model_work->base_matrix[3] == 100 &&
           s.enemies[2].model_work->joints[0].matrix[3] == 106 &&
           s.effect_pool->works[7].model_matrix->at(3) == 106 &&
           s.effect_pool->works[7].model_matrix->at(7) == 202 &&
           s.effect_pool->works[7].model_matrix->at(11) == 300 &&
           !previous_pool->works[7].model_matrix,
       "Source linked order must update shared caches and per-work matrices");
    auto self = linked();
    calculate_scenario_models(self, 1, 2, true, false);
    ck(self.enemies[2].model_work->joints[0].matrix[3] == 102 &&
           !self.enemies[3].model_matrix &&
           !self.effect_pool->works[7].model_matrix,
       "Self calculation must not calculate children/effects");
    auto links = linked();
    calculate_scenario_models(links, 1, 2, false, true);
    ck(links.enemies[3].model_matrix->at(3) == 100 &&
           links.enemies[4].model_matrix->at(3) == 102 &&
           links.effect_pool->works[7].model_matrix->at(3) == 104,
       "Link-only calculation uses retained parent joints");
    auto props = fixture();
    props.objects.resize(5);
    props.items.resize(1);
    props.objects[4] = props.enemies[3];
    props.objects[4].pose_owner.reset();
    props.objects[4].joint_pose = props.enemies[2].joint_pose;
    props.objects[4].model_work = props.enemies[2].model_work;
    props.items[0] = props.objects[4];
    props.items[0].linked_offset = {0, 3, 0};
    calculate_scenario_models(props, 1, 2, true, true);
    ck(props.objects[4].model_matrix && props.items[0].model_matrix &&
           props.items[0].model_matrix->at(7) ==
               props.objects[4].model_matrix->at(7) + 3,
       "Original linked object/item pools must calculate their matrices");
    auto invalid = linked();
    invalid.effect_pool =
        std::make_shared<NativeEffectPool>(*invalid.effect_pool);
    auto bad_pool = std::make_shared<NativeEffectPool>(*invalid.effect_pool);
    bad_pool->works[7].linked_joint = 999;
    invalid.effect_pool = bad_pool;
    bad([&] { calculate_scenario_models(invalid, 1, 2, true, true); });
    ck(invalid.enemies[2].model_work->joints[0].matrix[3] == 100 &&
           !invalid.enemies[3].model_matrix && invalid.effect_pool == bad_pool,
       "Malformed linked effect must reject before any model publication");
    auto retained = fixture();
    retained.enemies[2].flags |= 0x1000;
    retained.enemies[2].position.x = 999;
    calculate_scenario_models(retained, 1, 2, true, false);
    ck(retained.enemies[2].model_matrix->at(3) == 100,
       "Original retained base must survive calculation");
    for (uint8_t action : {0, 1, 2, 3, 4, 5, 6, 7}) {
      auto v = fixture();
      v.enemies[2].position.x = 400; // Retained cache is still at100.
      auto &aux = v.enemies[4];
      aux = v.enemies[3];
      aux.id = 31;
      aux.linked_joint = 0;
      auto pool = std::make_shared<NativeEffectPool>();
      pool->works[7].flags = 0x81;
      pool->works[7].linked_parent = 2;
      pool->works[7].linked_joint = 1;
      v.effect_pool = pool;
      select_scenario_motion(v, 1, 2, {1, 1, 4, 1, action, 6});
      const bool self =
          action == 0 || action == 3 || action == 5 || action == 7;
      const bool links =
          action == 0 || action == 4 || action == 6 || action == 7;
      const bool sample =
          action == 0 || action == 2 || action == 5 || action == 6;
      const double x = sample ? 2 : 0;
      ck(v.enemies[2].model_matrix->at(3) == (self ? 400 : 100) &&
             bool(v.enemies[3].model_matrix) == links &&
             bool(v.effect_pool->works[7].model_matrix) == links,
         "Original common24 immediate action must calculate exact self/link "
         "masks");
      if (links)
        ck(v.enemies[3].model_matrix->at(3) == (self ? 400 + x : 100) &&
               v.effect_pool->works[7].model_matrix->at(3) ==
                   (self ? 400 + x : 100) + 2 * x,
           "Immediate sampling/model/link order must use resulting shared "
           "pose");
    }
    std::cout << "Scripted model checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
