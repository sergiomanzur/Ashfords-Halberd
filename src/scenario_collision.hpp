#pragma once
#include "scenario.hpp"
namespace ashford {
// event.c bhWalAtariSet/bhEtcAtariSet/bhFlrAtariSet: static tables0/1/2.
void set_scenario_collision_visibility(ScenarioState &, uint8_t table,
                                       size_t index, bool enabled);
// Miscellaneous ATR records serve both collision and examination. Original
// commands14/35/37 must update both native views of the same source record.
// Bound collision records retain live stair reservations; unbound fixtures
// retain their standalone interaction storage.
const CollisionRecord &scenario_interaction(const ScenarioState &,
                                            size_t index);
void replace_scenario_interaction(ScenarioState &, size_t index,
                                  const CollisionRecord &);
} // namespace ashford
