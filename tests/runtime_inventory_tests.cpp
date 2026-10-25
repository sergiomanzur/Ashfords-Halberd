#include "runtime_view.hpp"
#include "inventory_engine.hpp"
#include "save_state.hpp"
#include <iostream>
#include <stdexcept>

using namespace ashford;
namespace {
void require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
}
int main(int argc, char **argv) {
  try {
    if (argc != 3) throw std::runtime_error("Expected assets and isolated output directory");
    const fs::path assets = argv[1];
    if (!fs::exists(assets / "RDX_LNK.AFS") || !fs::exists(assets / "SYSTEM.AFS")) {
      std::cout << "SKIP: user-owned assets absent\n";
      return 77;
    }
    SDL_SetMainReady();
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) == 0, SDL_GetError());
    struct Quit { ~Quit() { SDL_Quit(); } } quit;
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("Inventory integration", 0, 0, 320, 240, SDL_WINDOW_HIDDEN), SDL_DestroyWindow);
    require(bool(window), SDL_GetError());
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE), SDL_DestroyRenderer);
    require(bool(renderer), SDL_GetError());
    RuntimeView view(renderer.get(), assets / "RDX_LNK.AFS", assets / "SYSTEM.AFS", 0);
    view.set_audio_running(false);
    auto &state = view.state().scenario;
    require(state.inventory[0] == 2, "Header selects carried word 2");
    require(state.inventory[1] == ((55u << 16) | 1), "Extra lighter does not occupy pack");
    require(InventoryEngine::get_item_id(InventoryEngine::get_slot(state, 0)) == 9,
            "Experimental starter handgun uses original inventory ID");
    require(state.equipped_weapon == 4, "WeaponSet maps handgun item 9 to runtime weapon 4");

    // Isolate inventory consumption from the separate unfinished combat
    // controllers. This harness makes no original-encounter acceptance claim.
    for (auto &entity : state.enemies) entity.flags &= ~1u;
    const auto before = InventoryEngine::get_quantity(InventoryEngine::get_slot(state, 0));
    view.tick(0x10 | 0x100);
    view.tick(0);
    require(InventoryEngine::get_quantity(InventoryEngine::get_slot(state, 0)) == before - 1,
            "Runtime fire consumes carried ammunition");
    std::string message;
    require(InventoryEngine::equip_weapon(state, 0, message), "Toggle weapon off");
    require(InventoryEngine::equip_weapon(state, 0, message), "Re-equip weapon");
    require(InventoryEngine::get_quantity(InventoryEngine::get_slot(state, 0)) == before - 1,
            "Re-equip does not restore ammo");
    require(InventoryEngine::swap_slots(state, 0, 6), "Move equipped gun");
    require(state.inventory[0] == 8, "Equip selector follows gun moved to word 8");
    state.player_health = 90;
    state.player_state_flags |= 0x80000u;
    state.game_mode = 2;
    state.spray_use_count = 3;
    const auto inventory = state.inventory;
    const auto file = fs::path(argv[2]) / "inventory-v2.dat";
    view.save(file);
    state.inventory.fill(0);
    state.player_health = 10;
    state.player_state_flags &= ~0x80000u;
    state.game_mode = 0;
    view.load(file);
    require(view.state().scenario.inventory == inventory, "Live save restores exact inventory banks/selector");
    require(view.state().scenario.game_mode == 2, "Live save restores difficulty");
    require(view.state().scenario.spray_use_count == 3, "Live save restores recovery usage counter");
    require(view.state().scenario.player_health == 90, "Live save restores item health");
    require(view.state().scenario.player_state_flags & 0x80000u, "Live save restores actual poison flag");

    // Resource reconstruction in the other slice room must preserve ownership.
    auto cross_room = view.state();
    cross_room.scenario.room = 2;
    save_game_to_file(file, cross_room);
    view.load(file);
    require(view.state().scenario.room == 2, "Cross-room inventory restoration");
    require(view.state().scenario.inventory == inventory, "Cross-room inventory/selector retained");
    require(view.state().scenario.player_state_flags & 0x80000u, "Cross-room poison retained");
    const auto live_room = view.state().scenario.room;
    auto invalid_room = view.state();
    invalid_room.scenario.room = 255;
    save_game_to_file(file, invalid_room);
    bool rejected = false;
    try { view.load(file); } catch (const std::exception &) { rejected = true; }
    require(rejected, "Unavailable saved room rejected");
    require(view.state().scenario.room == live_room && view.state().scenario.inventory == inventory,
            "Failed room reconstruction preserves live inventory and room");
    std::cout << "runtime_inventory_tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAILED: " << e.what() << '\n';
    return 1;
  }
}
