#pragma once

#include <cstdint>
#include <string>

namespace ashford {
struct ScenarioState;

// Accept a confirmed (flags[10][0] & 0x10) pickup. Inventory insertion is
// staged; failure clears only the pending confirmation bit, leaving the pickup
// and inventory available for re-examination. Success sets the opcode-14
// acceptance bit and clears confirmation; the authored event then commits
// item/trigger/persistence changes. Quantities and variant bits come from the
// selected source catalog and game mode. Cancellation is handled by caller.
bool accept_selected_item(ScenarioState &state, std::string &message);
} // namespace ashford
