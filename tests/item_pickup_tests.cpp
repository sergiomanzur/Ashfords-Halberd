#include "inventory_engine.hpp"
#include "item_pickup.hpp"
#include "scenario.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace ashford;

namespace {
void check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

ScenarioState pickup(uint16_t id, uint8_t trigger_type = 4) {
  ScenarioState state;
  state.flags[10][0] = 0x10;
  state.selected_item_id = id;
  state.interaction_index = 2;
  state.items.resize(1);
  state.items[0].id = id;
  state.items[0].flags = 1;
  state.interactions.resize(3);
  auto &trigger = state.interactions[2];
  trigger.flags = 0xa5;
  trigger.type = trigger_type;
  trigger.parameters[0] = 0;
  return state;
}

void success_and_authored_persistence() {
  auto state = pickup(12);
  state.game_mode = 1;
  const auto old_item = state.items[0];
  const auto old_trigger = state.interactions[2];
  std::string message;
  check(accept_selected_item(state, message),
        "Ammo pickup should fit inventory");
  check(InventoryEngine::count_item(state, 12) == 30 &&
            !(state.flags[10][0] & 0x10) && (state.flags[10][0] & 0x800),
        "Success must add exactly requested quantity and signal event14");
  check(state.items[0].flags == old_item.flags &&
            state.items[0].state_flags == old_item.state_flags &&
            state.interactions[2].flags == old_trigger.flags &&
            state.flags[7][0] == 0,
        "GetItem acceptance leaves world and persistence for event14");

  // Event 14's count is the authored save-progress index. Its packed MSB bit
  // and trigger policy remain in the original script contract.
  run_scenario(Bytes{14, 0, 70, 0, 2, 0, 0}, state);
  check(state.flags[7][2] == (0x80000000u >> 6) &&
            state.items[0].state_flags == 0x1000000 &&
            state.items[0].flags == 1 &&
            state.interactions[2].flags == 0xa4,
        "Authored event14 commits original counter and world item state");

  auto restored = pickup(31);
  restored.flags[7][2] = 0x80000000u >> 6;
  run_scenario(Bytes{35, 0, 70, 0, 2, 0, 0}, restored);
  check(restored.items[0].state_flags == 0x1000000 &&
            restored.interactions[2].flags == 0xa4,
        "Authored event35 restores the same persisted pickup");
}

void non_ammo_quantity() {
  auto state = pickup(21);
  std::string message;
  check(accept_selected_item(state, message) &&
            InventoryEngine::count_item(state, 21) == 1,
        "Non-ammo pickup must retain authored quantity one");
}

void rejection_is_retryable() {
  std::string message;
  auto full = pickup(21);
  full.flags[10][0] |= 0x8000;
  for (size_t i = 0; i < InventoryEngine::kBaseCapacity; ++i)
    InventoryEngine::set_slot(full, i, InventoryEngine::make_slot(20, 1));
  const auto old_inventory = full.inventory;
  check(!accept_selected_item(full, message) &&
            full.inventory == old_inventory && (full.flags[10][0] & 0x10) == 0 &&
            !(full.flags[10][0] & 0x800) && full.items[0].flags == 1 &&
            full.interactions[2].flags == 0xa5 && full.flags[7][0] == 0,
        "Full inventory rejects without world/progress mutation");
  check(full.flags[10][0] & 0x8000,
        "Failed starting-item grant must preserve source cb_flg");

  auto stacked = pickup(12);
  stacked.game_mode = 1;
  InventoryEngine::set_slot(stacked, 0,
                            InventoryEngine::make_slot(12, 999));
  for (size_t i = 1; i < InventoryEngine::kBaseCapacity; ++i)
    InventoryEngine::set_slot(stacked, i, InventoryEngine::make_slot(20, 1));
  const auto stack_before = stacked.inventory;
  check(!accept_selected_item(stacked, message) &&
            stacked.inventory == stack_before && stacked.items[0].flags == 1,
        "Overflowing a full ammo stack must roll back staged insertion");

  auto malformed = pickup(21, 3);
  const auto flags_before = malformed.flags;
  check(!accept_selected_item(malformed, message) &&
            malformed.items[0].flags == 1 &&
            malformed.interactions[2].flags == 0xa5 &&
            malformed.flags[7] == flags_before[7] &&
            !(malformed.flags[10][0] & 0x10) &&
            !(malformed.flags[10][0] & 0x800),
        "Bad trigger must not grant or consume a pickup");

  auto invalid_mode = pickup(21);
  invalid_mode.game_mode = 4;
  const auto inventory_before = invalid_mode.inventory;
  check(!accept_selected_item(invalid_mode, message) &&
            invalid_mode.inventory == inventory_before &&
            invalid_mode.items[0].flags == 1 &&
            invalid_mode.interactions[2].flags == 0xa5 &&
            !(invalid_mode.flags[10][0] & 0x800),
        "Invalid quantity-table mode must leave pickup unconsumed");

}

void original_grant_words() {
  auto empty_card = pickup(82);
  empty_card.game_mode = 3;
  std::string card_message;
  check(accept_selected_item(empty_card, card_message) &&
        InventoryEngine::get_slot(empty_card, 0) == (82u << 16),
        "Source zero-quantity key grant remains an inventory record");
  std::string message;
  for (uint16_t id : {uint16_t(33), uint16_t(34), uint16_t(8)}) {
    auto claire = pickup(id);
    check(accept_selected_item(claire, message),
          "Claire zero-loaded weapon grants must be accepted");
    check(InventoryEngine::get_item_id(InventoryEngine::get_slot(claire, 0)) == id &&
              InventoryEngine::get_quantity(InventoryEngine::get_slot(claire, 0)) == 0,
          "Claire source weapon grant must preserve zero loaded quantity");
  }

  auto other_player = pickup(34);
  other_player.player_id = 1;
  other_player.game_mode = 0;
  check(accept_selected_item(other_player, message) &&
            InventoryEngine::get_quantity(InventoryEngine::get_slot(other_player, 0)) == 400,
        "Other player's item34 grant must retain authored loaded quantity");

  auto marked_variant = pickup(34);
  marked_variant.player_id = 1;
  marked_variant.game_mode = 3;
  check(accept_selected_item(marked_variant, message),
        "Packed item34 mode3 source grant must be accepted");
  check(InventoryEngine::get_slot(marked_variant, 0) ==
            ((uint32_t(0x822) << 16) | 400),
        "Packed getbulletmax high-word variant marker must survive insertion");

  auto extra = pickup(55);
  extra.player_id = 1;
  for (size_t slot = 0; slot < InventoryEngine::kBaseCapacity; ++slot)
    InventoryEngine::set_slot(extra, slot, InventoryEngine::make_slot(21, 1));
  std::array<uint32_t, InventoryEngine::kBaseCapacity> before{};
  for (size_t slot = 0; slot < before.size(); ++slot)
    before[slot] = InventoryEngine::get_slot(extra, slot);
  check(accept_selected_item(extra, message),
        "ID55 must grant into its dedicated bank word without a free pack slot");
  bool pack_unchanged = true;
  for (size_t slot = 0; slot < before.size(); ++slot)
    pack_unchanged &= before[slot] == InventoryEngine::get_slot(extra, slot);
  check(InventoryEngine::get_item_id(InventoryEngine::get_extra_slot(extra)) == 55 &&
            pack_unchanged &&
            InventoryEngine::get_capacity(extra) == InventoryEngine::kBaseCapacity,
        "ID55 extra grant must preserve ordinary pack capacity and contents");

  for (const auto &[id, expected] :
       {std::pair<uint16_t, uint16_t>{9, 12}, {3, 5}}) {
    auto infinite = pickup(id);
    infinite.flags[10][0] |= 0x8000;
    check(accept_selected_item(infinite, message),
          "Infinite starting weapon grant must be accepted");
    check(InventoryEngine::get_quantity(InventoryEngine::get_slot(infinite, 0)) == expected,
          "Infinite starting weapon grant must apply source magazine adjustment");
    check(!(infinite.flags[10][0] & 0x8000),
          "Successful starting item grant must consume cb_flg 0x8000");
    if (id == 9)
      check(infinite.equipped_weapon == ItemDatabase::weapon_index(9),
            "Infinite ID9 grant must apply source WeaponSet selection");
  }
}

void room_specific_grants() {
  std::string message;
  auto overflow_room = pickup(37);
  overflow_room.stage = 5; overflow_room.room = 51;
  InventoryEngine::set_slot(overflow_room, 0, InventoryEngine::make_slot(37, 998));
  check(accept_selected_item(overflow_room, message), "Reserve overflow pickup accepted");
  check(InventoryEngine::get_quantity(InventoryEngine::get_slot(overflow_room, 0)) == 999 &&
        !(InventoryEngine::get_slot(overflow_room, 1) & 0x08000000u),
        "Overflow remainder uses source num1 branch without room marker");

  auto first_room_empty = pickup(31);
  first_room_empty.room = 1;
  check(accept_selected_item(first_room_empty, message) &&
            InventoryEngine::get_quantity(InventoryEngine::get_slot(first_room_empty, 0)) == 1,
        "Room 1 ID31 pickup without an existing stack must grant one round");

  auto first_room = pickup(31);
  first_room.room = 1;
  InventoryEngine::set_slot(first_room, 0,
                            InventoryEngine::make_slot(31, 5));
  check(accept_selected_item(first_room, message) &&
            InventoryEngine::get_quantity(InventoryEngine::get_slot(first_room, 0)) == 6,
        "Room 1 ID31 pickup must add exactly one to an existing reserve");

  auto special_handgun = pickup(4);
  special_handgun.stage = 9;
  special_handgun.room = 26;
  special_handgun.inventory[224] = 0x1234000du;
  check(accept_selected_item(special_handgun, message) &&
            InventoryEngine::get_quantity(InventoryEngine::get_slot(special_handgun, 0)) == 13,
        "Room 926 ID4 pickup must use itm[224]'s authored low byte");

  auto room_variant = pickup(37);
  room_variant.stage = 5;
  room_variant.room = 51;
  check(accept_selected_item(room_variant, message) &&
            (InventoryEngine::get_slot(room_variant, 0) & 0x08000000u),
        "Room 551 ID37 pickup must preserve the infinite-ammo marker");

  auto always_variant = pickup(11);
  check(accept_selected_item(always_variant, message) &&
            (InventoryEngine::get_slot(always_variant, 0) & 0x08000000u),
        "ID11 pickup must always carry its source infinite-ammo marker");

  auto existing_infinite = pickup(37);
  existing_infinite.stage = 5;
  existing_infinite.room = 51;
  InventoryEngine::set_slot(existing_infinite, 0,
                            InventoryEngine::make_slot(37, 11));
  const auto previous_other_slot = InventoryEngine::get_slot(existing_infinite, 1);
  check(accept_selected_item(existing_infinite, message) &&
            InventoryEngine::get_quantity(InventoryEngine::get_slot(existing_infinite, 0)) == 14 &&
            !(InventoryEngine::get_slot(existing_infinite, 0) & 0x08000000u) &&
            InventoryEngine::get_slot(existing_infinite, 1) == previous_other_slot,
        "Room 551 marker applies only on a new record, not a regular stack");

  auto overflow_variant = pickup(37);
  overflow_variant.stage = 5;
  overflow_variant.room = 51;
  InventoryEngine::set_slot(overflow_variant, 0,
                            InventoryEngine::make_slot(37, 999));
  for (size_t slot = 1; slot < InventoryEngine::kBaseCapacity; ++slot)
    InventoryEngine::set_slot(overflow_variant, slot,
                              InventoryEngine::make_slot(20, 1));
  const auto overflow_inventory = overflow_variant.inventory;
  check(!accept_selected_item(overflow_variant, message) &&
            overflow_variant.inventory == overflow_inventory &&
            !(InventoryEngine::get_slot(overflow_variant, 0) & 0x08000000u) &&
            !(overflow_variant.flags[10][0] & 0x800u) &&
            overflow_variant.items[0].flags == 1 &&
            overflow_variant.interactions[2].flags == 0xa5,
        "Room 551 overflow leaves the regular stack and pickup retryable");

  auto premarked = pickup(37);
  premarked.stage = 5;
  premarked.room = 51;
  InventoryEngine::set_slot(premarked, 0,
                            (uint32_t(0x825) << 16) | 11u);
  check(accept_selected_item(premarked, message) &&
            InventoryEngine::get_quantity(InventoryEngine::get_slot(premarked, 0)) == 11 &&
            (InventoryEngine::get_slot(premarked, 0) & 0x08000000u),
        "Existing infinite stack consumes the pickup without adding rounds");
}
} // namespace

int main() {
  try {
    success_and_authored_persistence();
    non_ammo_quantity();
    rejection_is_retryable();
    original_grant_words();
    room_specific_grants();
    std::cout << "Item pickup checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
