#include "scenario.hpp"
#include <iostream>
#include <stdexcept>
using namespace ashford;
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
int main() {
  try {
    const Bytes request_check{16, 65, 5, 1, 0, 0, 0, 0, 0};
    ScenarioState state;
    state.selected_item_id = 65;
    run_scenario(request_check, state);
    check(state.flags[1][0] == 0, "Item possession alone does not authorize use");
    state.flags[10][0] = 0x400u | 0x80000000u;
    run_scenario(request_check, state);
    check(state.flags[1][0] == 0x80000000u, "Original use check sees confirmed matching item");
    run_scenario(Bytes{15, 0, 0}, state);
    check(state.flags[10][0] == 0x80000000u, "Use clear preserves unrelated flags");
    state.flags[1][0] = 0;
    state.flags[10][0] |= 0x400;
    state.selected_item_id = 66;
    run_scenario(request_check, state);
    check(state.flags[1][0] == 0, "Different selected key does not satisfy event");

    state.player_id = 1;
    state.inventory[16] = 4;
    state.inventory[17] = 55u << 16 | 1;
    state.inventory[18] = 65u << 16 | 1;
    state.inventory[19] = 21u << 16 | 1;
    state.inventory[20] = 9u << 16 | 11;
    state.equipped_weapon = 4;
    const auto other_bank = state.inventory[0];
    run_scenario(Bytes{49, 65, 0}, state);
    check(state.inventory[17] == (55u << 16 | 1), "Script loss preserves extra slot");
    check(state.inventory[18] == (21u << 16 | 1) &&
          state.inventory[19] == (9u << 16 | 11) && state.inventory[16] == 3,
          "Script loss compacts the active player's bank and moves the selector");
    check(state.inventory[0] == other_bank && state.equipped_weapon == 4,
          "Script loss preserves other player and equipped gun identity");
    run_scenario(Bytes{49, 9, 0}, state);
    check(state.inventory[16] == 0 && state.equipped_weapon == 0,
          "Script loss of equipped item unarms player");
    std::cout << "inventory_event_tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAILED: " << e.what() << '\n';
    return 1;
  }
}
