#pragma once
#include "scenario.hpp"
namespace ashford {
struct ScenarioMotionSelection {
  uint8_t speed, selector, motion, event_mode, immediate, blend;
};
// event.c Common_controll24, MdlPut.c bhCalcModel, pwksub.c bhCalcLinkModel.
void select_scenario_motion(ScenarioState &, uint8_t group, size_t work,
                            const ScenarioMotionSelection &);
void calculate_scenario_models(ScenarioState &, uint8_t group, size_t work,
                               bool self, bool links);
// subpl.c em_sce / bhEne_Event. Caller retains the enclosing controller order.
void tick_scripted_enemy(ScenarioState &, size_t work);
} // namespace ashford
