#include "scenario_collision.hpp"
#include <stdexcept>
// Native adaptation of MIT recvx-decomp event.c static ATR commands.
namespace ashford {
const CollisionRecord &scenario_interaction(const ScenarioState &s,
                                            size_t index) {
  if (index >= s.interactions.size())
    throw std::runtime_error("Scenario interaction index outside room");
  if (s.collision && s.collision->miscellaneous.size() != s.interactions.size())
    throw std::runtime_error("Inconsistent scenario miscellaneous binding");
  // Stair controllers reserve/release the live collision record directly.
  // Never publish stale interaction fields over those current attributes.
  return s.collision ? s.collision->miscellaneous[index]
                     : s.interactions[index];
}
void replace_scenario_interaction(ScenarioState &s, size_t index,
                                  const CollisionRecord &replacement) {
  scenario_interaction(s, index); // Validate before either copy changes.
  s.interactions[index] = replacement;
  if (s.collision)
    s.collision->miscellaneous[index] = replacement;
}
void set_scenario_collision_visibility(ScenarioState &s, uint8_t table,
                                       size_t index, bool enabled) {
  if (!s.collision || table > 2)
    throw std::runtime_error("Missing or invalid scenario collision table");
  auto &records = table == 0   ? s.collision->walls
                  : table == 1 ? s.collision->miscellaneous
                               : s.collision->floors;
  if (index >= records.size())
    throw std::runtime_error("Scenario collision index outside static table");
  if (table == 1) {
    auto replacement = scenario_interaction(s, index);
    replacement.flags = enabled ? uint8_t(replacement.flags | 1u)
                                : uint8_t(replacement.flags & ~1u);
    replace_scenario_interaction(s, index, replacement);
  } else {
    auto &record = records[index];
    record.flags =
        enabled ? uint8_t(record.flags | 1u) : uint8_t(record.flags & ~1u);
  }
}
} // namespace ashford
