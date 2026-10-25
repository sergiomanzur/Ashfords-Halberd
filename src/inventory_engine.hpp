#pragma once
#include "item_database.hpp"
#include "scenario.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace ashford {

class InventoryEngine {
public:
  static constexpr size_t kBaseCapacity = 8;
  static constexpr size_t kExpandedCapacity = 10;
  static constexpr size_t kExtraSlot = 10; // bank word 1, separate from item box
  static constexpr uint16_t kMaxPlayerHealth = 160;

  static size_t get_capacity(const ScenarioState &state);
  static uint32_t get_slot(const ScenarioState &state, size_t slot_idx);
  static void set_slot(ScenarioState &state, size_t slot_idx, uint32_t value);
  static uint32_t get_extra_slot(const ScenarioState &state);
  static void set_extra_slot(ScenarioState &state, uint32_t value);
  static uint16_t get_item_id(uint32_t slot_value);
  static uint16_t get_quantity(uint32_t slot_value);
  static uint32_t make_slot(uint16_t item_id, uint16_t qty);
  static bool add_item_word(ScenarioState &state, uint32_t raw_item_word);

  static bool is_equipped(const ScenarioState &state, size_t slot_idx);
  static bool can_use_item(const ScenarioState &state, size_t slot_idx);
  static bool equip_extra_item(ScenarioState &state, std::string &message);
  static bool use_extra_item(ScenarioState &state, std::string &message);
  // The retail selector word stores a one-based word index in the active
  // player's 16-word inventory bank. The returned value is a zero-based UI slot.
  static std::optional<size_t> equipped_slot(const ScenarioState &state);
  static bool consume_equipped_ammo(ScenarioState &state, uint16_t amount = 1);
  static bool equip_weapon(ScenarioState &state, size_t slot_idx,
                           std::string &message);
  static bool use_item(ScenarioState &state, size_t slot_idx,
                       std::string &message);
  static bool can_combine(const ScenarioState &state, size_t slot_a,
                          size_t slot_b);
  static bool combine_slots(ScenarioState &state, size_t slot_a, size_t slot_b,
                            std::string &message);
  static bool swap_slots(ScenarioState &state, size_t slot_a, size_t slot_b);
  static bool add_item(ScenarioState &state, uint16_t item_id,
                       uint16_t quantity = 1);
  static int count_item(const ScenarioState &state, uint16_t item_id);
  static bool remove_item(ScenarioState &state, uint16_t item_id,
                          uint16_t quantity = 1);

  static std::string get_condition_string(int32_t health, bool is_poisoned);
  static uint32_t get_condition_color(int32_t health, bool is_poisoned);
};

} // namespace ashford
