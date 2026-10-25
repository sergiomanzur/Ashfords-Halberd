#include "inventory_engine.hpp"
#include "inventory_ui.hpp"
#include "item_database.hpp"
#include "scenario.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace ashford;

#define REQUIRE(expr) do { if (!(expr)) throw std::runtime_error( \
  std::string("inventory regression failed: ") + #expr + " at line " + std::to_string(__LINE__)); } while (false)

static void test_banks_and_extra_slot() {
  ScenarioState s;
  REQUIRE(InventoryEngine::get_capacity(s) == 8);
  const uint32_t glock = InventoryEngine::make_slot(2, 12);
  InventoryEngine::set_slot(s, 0, glock);
  REQUIRE(s.inventory[2] == glock); // word 0 selector, word 1 extra, grid begins at 2
  REQUIRE(InventoryEngine::get_slot(s, 0) == glock);
  s.player_id = 1;
  InventoryEngine::set_slot(s, 0, InventoryEngine::make_slot(3, 4));
  REQUIRE(s.inventory[18] == InventoryEngine::make_slot(3, 4));
  REQUIRE(InventoryEngine::get_slot(s, 0) == s.inventory[18]);
  std::string msg;
  InventoryEngine::set_extra_slot(s, InventoryEngine::make_slot(55, 1));
  REQUIRE(InventoryEngine::get_slot(s, InventoryEngine::kExtraSlot) == s.inventory[17]);
  REQUIRE(InventoryEngine::equip_extra_item(s, msg));
  REQUIRE(InventoryEngine::equipped_slot(s) == InventoryEngine::kExtraSlot);
  REQUIRE(s.equipped_weapon == 1);
  REQUIRE(InventoryEngine::equip_extra_item(s, msg));
  REQUIRE(!InventoryEngine::equipped_slot(s));
  REQUIRE(InventoryEngine::get_slot(s, 0) == 0 || s.inventory[16] == 0);
  s.player_id = 24;
  REQUIRE(!InventoryEngine::add_item(s, 21, 1));
}

static void test_weapon_ownership_and_ammo() {
  ScenarioState s;
  std::string msg;
  InventoryEngine::set_slot(s, 0, InventoryEngine::make_slot(2, 9));
  InventoryEngine::set_slot(s, 1, InventoryEngine::make_slot(2, 7));
  REQUIRE(InventoryEngine::equip_weapon(s, 0, msg));
  REQUIRE(s.inventory[0] == 2); // one-based local bank word index, never packed weapon id
  REQUIRE(InventoryEngine::is_equipped(s, 0));
  REQUIRE(InventoryEngine::consume_equipped_ammo(s));
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(s, 0)) == 8);
  InventoryEngine::set_slot(s, 0, InventoryEngine::make_slot(2 | 0x0800, 3));
  REQUIRE(InventoryEngine::consume_equipped_ammo(s));
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(s, 0)) == 3);
  REQUIRE(InventoryEngine::swap_slots(s, 0, 1));
  REQUIRE(InventoryEngine::equipped_slot(s) == 1);
  REQUIRE(s.equipped_weapon == ItemDatabase::weapon_index(2));
  InventoryEngine::set_slot(s, 1, 0); // remove the selected duplicate gun
  REQUIRE(!InventoryEngine::equipped_slot(s));
  REQUIRE(s.equipped_weapon == 0);
}

static void test_transactional_capacity_and_quantity() {
  ScenarioState s;
  // ID 2 occupies two physical inventory positions (attribute 0x100).
  InventoryEngine::set_slot(s, 0, InventoryEngine::make_slot(2, 5));
  for (size_t i = 1; i < 7; ++i)
    InventoryEngine::set_slot(s, i, InventoryEngine::make_slot(21, 1));
  const auto before = s.inventory;
  REQUIRE(!InventoryEngine::add_item(s, 3, 1));
  REQUIRE(s.inventory == before); // two-slot insertion must fail atomically

  ScenarioState stack;
  REQUIRE(InventoryEngine::add_item(stack, 31, 998));
  REQUIRE(InventoryEngine::add_item(stack, 31, 2));
  REQUIRE(InventoryEngine::count_item(stack, 31) == 1000);
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(stack, 0)) == 999);
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(stack, 1)) == 1);
  ScenarioState powder_reserve;
  REQUIRE(InventoryEngine::add_item_word(powder_reserve, InventoryEngine::make_slot(42, 300)));
  REQUIRE(InventoryEngine::add_item_word(powder_reserve, InventoryEngine::make_slot(42, 1)));
  REQUIRE(InventoryEngine::count_item(powder_reserve, 42) == 301);
  REQUIRE((ItemDatabase::get(42).original_attributes & 0x200u) == 0);
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(powder_reserve, 0)) == 300);
  REQUIRE(InventoryEngine::get_slot(powder_reserve, 1) == InventoryEngine::make_slot(42, 1));
  ScenarioState weapon_grant;
  REQUIRE(InventoryEngine::add_item_word(weapon_grant, InventoryEngine::make_slot(34, 400)));
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(weapon_grant, 0)) == 400);
  REQUIRE(InventoryEngine::get_slot(weapon_grant, 1) == 0);
  ScenarioState custom_handgun_grant;
  REQUIRE(InventoryEngine::add_item_word(custom_handgun_grant,
      InventoryEngine::make_slot(4, 13)));
  REQUIRE(InventoryEngine::get_item_id(InventoryEngine::get_slot(custom_handgun_grant, 0)) == 4);
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(custom_handgun_grant, 0)) == 13);
  REQUIRE(InventoryEngine::get_slot(custom_handgun_grant, 1) == 0);
  ScenarioState infinite;
  InventoryEngine::set_slot(infinite, 0, InventoryEngine::make_slot(31 | 0x0800, 40));
  REQUIRE(InventoryEngine::add_item(infinite, 31, 12));
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(infinite, 0)) == 40);
  REQUIRE(InventoryEngine::get_slot(infinite, 1) == 0);
  ScenarioState mark_infinite;
  InventoryEngine::set_slot(mark_infinite, 0, InventoryEngine::make_slot(31, 40));
  REQUIRE(InventoryEngine::add_item_word(mark_infinite,
      InventoryEngine::make_slot(31 | 0x0800, 12)));
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(mark_infinite, 0)) == 52);
  REQUIRE((InventoryEngine::get_slot(mark_infinite, 0) & 0x08000000u) == 0);
  REQUIRE(InventoryEngine::get_slot(mark_infinite, 1) == 0);
  ScenarioState full_infinite;
  InventoryEngine::set_slot(full_infinite, 0,
      InventoryEngine::make_slot(31 | 0x0800, 999));
  REQUIRE(InventoryEngine::add_item(full_infinite, 31, 12));
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(full_infinite, 0)) == 999);
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(full_infinite, 1)) == 12);
  REQUIRE((InventoryEngine::get_slot(full_infinite, 1) & 0x08000000u) == 0);
  const auto stack_before = stack.inventory;
  REQUIRE(!InventoryEngine::add_item(stack, 0x9c, 1));
  REQUIRE(stack.inventory == stack_before);
  REQUIRE(InventoryEngine::make_slot(0x9c, 1) == 0);
  REQUIRE(InventoryEngine::make_slot(0x1006, 0) == 0x10060000u);
}

static void test_healing_and_condition_boundaries() {
  ScenarioState s;
  std::string msg;
  s.player_health = 60;
  s.player_state_flags |= 0x80000;
  s.flags[10][0] |= 0x80000; // cb flag with same bit is unrelated to poison
  InventoryEngine::set_slot(s, 0, InventoryEngine::make_slot(21, 1));
  REQUIRE(InventoryEngine::can_use_item(s, 0));
  REQUIRE(InventoryEngine::use_item(s, 0, msg));
  REQUIRE(s.player_health == 110);
  REQUIRE((s.player_state_flags & 0x80000) != 0);
  REQUIRE((s.flags[10][0] & 0x80000) != 0);
  InventoryEngine::set_slot(s, 1, InventoryEngine::make_slot(23, 1));
  REQUIRE(InventoryEngine::use_item(s, 1, msg));
  REQUIRE((s.player_state_flags & 0x80000) == 0);
  s.player_health = 160;
  InventoryEngine::set_slot(s, 2, InventoryEngine::make_slot(21, 1));
  REQUIRE(InventoryEngine::can_use_item(s, 2));
  REQUIRE(InventoryEngine::use_item(s, 2, msg));
  REQUIRE(InventoryEngine::get_slot(s, 2) == 0);
  InventoryEngine::set_slot(s, 3, InventoryEngine::make_slot(23, 1));
  REQUIRE(InventoryEngine::can_use_item(s, 3));
  REQUIRE(InventoryEngine::use_item(s, 3, msg));
  REQUIRE(InventoryEngine::get_slot(s, 3) == 0);
  s.player_state_flags |= 0x00280000u;
  InventoryEngine::set_slot(s, 4, InventoryEngine::make_slot(23, 1));
  REQUIRE(InventoryEngine::can_use_item(s, 4));
  REQUIRE(!InventoryEngine::use_item(s, 4, msg));
  REQUIRE((s.player_state_flags & 0x80000u) == 0);
  REQUIRE((s.player_state_flags & 0x200000u) != 0);
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(s, 4)) == 1);

  REQUIRE(InventoryEngine::get_condition_string(120, false) == "FINE");
  REQUIRE(InventoryEngine::get_condition_color(120, false) == 0x20d040);
  REQUIRE(InventoryEngine::get_condition_string(60, false) == "CAUTION");
  REQUIRE(InventoryEngine::get_condition_color(60, false) == 0xe0c020);
  REQUIRE(InventoryEngine::get_condition_color(30, false) == 0xe08020);
  REQUIRE(InventoryEngine::get_condition_string(29, false) == "DANGER");

  ScenarioState hard;
  hard.game_mode = 2; hard.player_health = 1;
  InventoryEngine::set_slot(hard, 0, InventoryEngine::make_slot(20, 1));
  REQUIRE(InventoryEngine::use_item(hard, 0, msg));
  REQUIRE(hard.player_health == 320);
  REQUIRE(hard.spray_use_count == 1);
  ScenarioState full_spray;
  full_spray.player_health = 160;
  InventoryEngine::set_slot(full_spray, 0, InventoryEngine::make_slot(20, 1));
  REQUIRE(InventoryEngine::use_item(full_spray, 0, msg));
  REQUIRE(full_spray.player_health == 160);
  REQUIRE(full_spray.spray_use_count == 1);
}

static void test_original_inventory_use_request() {
  ScenarioState s;
  RoomCollision collision;
  CollisionRecord area;
  area.flags = 1;
  area.type = 0;
  area.parameters[0] = 12;
  collision.floors.push_back(area);
  s.collision = collision;
  s.floor_index = 0;
  s.flags[10][0] |= 0x200u;
  InventoryEngine::set_slot(s, 0, InventoryEngine::make_slot(12, 14));
  REQUIRE(InventoryEngine::can_use_item(s, 0));
  std::string msg;
  REQUIRE(InventoryEngine::use_item(s, 0, msg));
  REQUIRE(s.selected_item_id == 12);
  REQUIRE((s.flags[10][0] & 0x400u) != 0);
  REQUIRE(InventoryEngine::get_quantity(InventoryEngine::get_slot(s, 0)) == 14);
  s.flags[10][0] &= ~0x200u;
  REQUIRE(!InventoryEngine::can_use_item(s, 0));
}

static void test_inventory_ui_extra_slot_and_key_use() {
  ScenarioState s;
  InventoryEngine::set_extra_slot(s, InventoryEngine::make_slot(55, 1));
  InventoryUI ui;
  ui.open();
  ui.handle_key_down(SDLK_x, s);
  REQUIRE(ui.selected_slot() == InventoryEngine::kExtraSlot);
  ui.handle_key_down(SDLK_RETURN, s);
  REQUIRE(ui.current_state() == InventoryState::CommandMenu);
  ui.handle_key_down(SDLK_RETURN, s);
  REQUIRE(InventoryEngine::equipped_slot(s) == InventoryEngine::kExtraSlot);
  REQUIRE(s.equipped_weapon == 1);
  ui.handle_key_down(SDLK_RETURN, s);
  REQUIRE(ui.current_state() == InventoryState::CommandMenu);
  ui.handle_key_down(SDLK_RETURN, s);
  REQUIRE(!InventoryEngine::equipped_slot(s));

  RoomCollision collision;
  CollisionRecord area;
  area.flags = 1; area.type = 0; area.parameters[0] = 65;
  collision.floors.push_back(area);
  s.collision = collision; s.floor_index = 0; s.flags[10][0] |= 0x200u;
  REQUIRE(ItemDatabase::is_key_item(65));
  InventoryEngine::set_slot(s, 0, InventoryEngine::make_slot(65, 1));
  ui.open();
  ui.handle_key_down(SDLK_RETURN, s);
  REQUIRE(ui.current_state() == InventoryState::CommandMenu);
  ui.handle_key_down(SDLK_RETURN, s);
  REQUIRE(!ui.is_open());
  REQUIRE((s.flags[10][0] & 0x400u) != 0);
  REQUIRE(s.selected_item_id == 65);
}

static void test_combinations_are_source_table_driven() {
  ScenarioState s;
  std::string msg;
  // Find an exact recipe directly from the retail table and validate its row
  // execution through the engine; this avoids asserting a guessed recipe.
  const auto &table = ItemDatabase::original_combinations();
  bool ran = false;
  for (uint16_t owner = 1; owner < 156 && !ran; ++owner) {
    const int offset = ItemDatabase::get(owner).combination_offset;
    if (offset < 0 || static_cast<size_t>(offset) >= table.size()) continue;
    const int count = table[static_cast<size_t>(offset)];
    for (int i = 0; i < count && !ran; ++i) {
      const size_t row = static_cast<size_t>(offset + 1 + i * 3);
      if (row + 2 >= table.size()) break;
      const uint16_t input = static_cast<uint16_t>(table[row]);
      if (!ItemDatabase::is_valid(input) || table[row + 2] < 0 || table[row + 2] > 11) continue;
      InventoryEngine::set_slot(s, 0, InventoryEngine::make_slot(input, 1));
      InventoryEngine::set_slot(s, 1, InventoryEngine::make_slot(owner, 1));
      if (!InventoryEngine::can_combine(s, 0, 1)) continue;
      const auto before = s.inventory;
      ran = InventoryEngine::combine_slots(s, 0, 1, msg);
      if (!ran) REQUIRE(s.inventory == before);
    }
  }
  REQUIRE(ran);

  bool checked_variant_target = false;
  for (uint16_t owner = 152; owner <= 155 && !checked_variant_target; ++owner) {
    const int offset = ItemDatabase::get(owner).combination_offset;
    if (offset < 0) continue;
    const int count = table[static_cast<size_t>(offset)];
    for (int i = 0; i < count && !checked_variant_target; ++i) {
      const size_t row = static_cast<size_t>(offset + 1 + i * 3);
      const int raw_input = table[row];
      if (raw_input <= 0 || !ItemDatabase::is_valid(static_cast<uint16_t>(raw_input))) continue;
      const uint16_t variant_word = owner == 152 ? 0x1006 : owner == 153 ? 0x2006 :
                                   owner == 154 ? 0x4006 : 0x2007;
      ScenarioState variant;
      InventoryEngine::set_slot(variant, 0, InventoryEngine::make_slot(static_cast<uint16_t>(raw_input), 1));
      InventoryEngine::set_slot(variant, 1, InventoryEngine::make_slot(variant_word, 1));
      REQUIRE(InventoryEngine::can_combine(variant, 0, 1));
      InventoryEngine::set_slot(variant, 0, InventoryEngine::make_slot(variant_word, 1));
      InventoryEngine::set_slot(variant, 1, InventoryEngine::make_slot(static_cast<uint16_t>(raw_input), 1));
      const int reverse_offset = ItemDatabase::get(static_cast<uint16_t>(raw_input)).combination_offset;
      bool reverse_recipe_present = false;
      if (reverse_offset >= 0) {
        const int reverse_count = table[static_cast<size_t>(reverse_offset)];
        for (int j = 0; j < reverse_count; ++j)
          if (table[static_cast<size_t>(reverse_offset + 1 + j * 3)] == owner)
            reverse_recipe_present = true;
      }
      REQUIRE(InventoryEngine::can_combine(variant, 0, 1) == reverse_recipe_present);
      checked_variant_target = true;
    }
  }
  REQUIRE(checked_variant_target);

  ScenarioState duplicate_handgun;
  InventoryEngine::set_slot(duplicate_handgun, 0, InventoryEngine::make_slot(19, 1));
  InventoryEngine::set_slot(duplicate_handgun, 1, InventoryEngine::make_slot(9, 8));
  InventoryEngine::set_slot(duplicate_handgun, 2, InventoryEngine::make_slot(9, 4));
  std::string equip_message;
  REQUIRE(InventoryEngine::equip_weapon(duplicate_handgun, 2, equip_message));
  REQUIRE(InventoryEngine::can_combine(duplicate_handgun, 0, 1));
  REQUIRE(InventoryEngine::combine_slots(duplicate_handgun, 0, 1, msg));
  REQUIRE(InventoryEngine::get_item_id(InventoryEngine::get_slot(duplicate_handgun, 1)) == 10);
  REQUIRE(InventoryEngine::equipped_slot(duplicate_handgun) == 1);
  REQUIRE(duplicate_handgun.equipped_weapon == ItemDatabase::weapon_index(10));
  REQUIRE((duplicate_handgun.flags[11][0] & 0x10000000u) == 0);
  InventoryEngine::set_slot(duplicate_handgun, 3, InventoryEngine::make_slot(131, 12));
  REQUIRE(InventoryEngine::equip_weapon(duplicate_handgun, 3, msg));
  REQUIRE((duplicate_handgun.flags[11][0] & 0x10000000u) != 0);
  REQUIRE(InventoryEngine::equip_weapon(duplicate_handgun, 3, msg));
  REQUIRE((duplicate_handgun.flags[11][0] & 0x10000000u) == 0);
}

int main() {
  try {
    test_banks_and_extra_slot();
    test_weapon_ownership_and_ammo();
    test_transactional_capacity_and_quantity();
    test_healing_and_condition_boundaries();
    test_original_inventory_use_request();
    test_inventory_ui_extra_slot_and_key_use();
    test_combinations_are_source_table_driven();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  std::cout << "Inventory mechanics tests passed.\n";
  return 0;
}
