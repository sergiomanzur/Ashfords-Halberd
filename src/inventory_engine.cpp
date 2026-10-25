#include "inventory_engine.hpp"
#include "inventory_combinations.hpp"
#include "item_use_area.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace ashford {
namespace {
constexpr size_t kPlayerBankWords = 16;
constexpr uint32_t kExpandedFlag = 0x08000000u;
constexpr uint32_t kPoisonFlag = 0x00080000u;

size_t bank_base(const ScenarioState &state) {
  return static_cast<size_t>(state.player_id) * kPlayerBankWords;
}

size_t max_health(const ScenarioState &state) {
  return state.game_mode == 2 ? 320u : 160u;
}

bool has_equipped_weapon_behavior(uint32_t word) {
  return ItemDatabase::weapon_index(static_cast<uint16_t>(word >> 16)) >= 0;
}

uint16_t ammo_flag(uint32_t word) {
  return static_cast<uint16_t>((word >> 16) & 0xff00u);
}

bool source_use_area_matches(const ScenarioState &state, uint16_t item_id) {
  const CollisionRecord *area = selected_inventory_use_area(state);
  if (!area) return false;
  return std::find(area->parameters.begin(), area->parameters.end(),
                   static_cast<uint8_t>(item_id)) != area->parameters.end();
}

struct Recipe { uint16_t input = 0, output = 0; uint8_t mode = 0; size_t row = 0; };

uint16_t normalized_recipe_id(uint32_t word) {
  uint16_t id = InventoryEngine::get_item_id(word);
  const uint16_t variant = static_cast<uint16_t>((word >> 16) & 0x7000u);
  if (id == 6) {
    if (variant == 0x1000) return 152;
    if (variant == 0x2000) return 153;
    if (variant == 0x4000) return 154;
  } else if (id == 7 && variant == 0x2000) {
    return 155;
  }
  return id;
}

std::optional<Recipe> find_recipe(uint16_t owner, uint16_t input) {
  const auto &item = ItemDatabase::get(owner);
  const int offset = item.combination_offset;
  if (offset < 0) return std::nullopt;
  const auto &table = ItemDatabase::original_combinations();
  if (static_cast<size_t>(offset) >= table.size()) return std::nullopt;
  const int count = table[static_cast<size_t>(offset)];
  if (count < 0 || count > 136) return std::nullopt;
  for (int i = 0; i < count; ++i) {
    const size_t pos = static_cast<size_t>(offset + 1 + i * 3);
    if (pos + 2 >= table.size()) break;
    if (table[pos] == static_cast<int16_t>(input))
      return Recipe{input, static_cast<uint16_t>(table[pos + 1]),
                    static_cast<uint8_t>(table[pos + 2]), pos};
  }
  return std::nullopt;
}

uint32_t encode(uint16_t id, uint16_t qty, uint16_t type_flags = 0) {
  return (static_cast<uint32_t>(id | type_flags) << 16) | qty;
}

void clear_selector_if_removed(ScenarioState &state, size_t absolute_word) {
  const size_t base = bank_base(state);
  if (absolute_word >= base && state.inventory[base] == absolute_word - base) {
    const uint16_t removed_id = InventoryEngine::get_item_id(state.inventory[absolute_word]);
    state.inventory[base] = 0;
    state.equipped_weapon = 0;
    if (removed_id == 131) state.flags[11][0] &= ~0x10000000u;
  }
}

} // namespace

size_t InventoryEngine::get_capacity(const ScenarioState &state) {
  return (state.flags[11][0] & kExpandedFlag) ? kExpandedCapacity
                                               : kBaseCapacity;
}

uint32_t InventoryEngine::get_slot(const ScenarioState &state, size_t slot_idx) {
  if (slot_idx == kExtraSlot) return get_extra_slot(state);
  const size_t absolute = bank_base(state) + slot_idx + 2;
  if (slot_idx >= get_capacity(state) || absolute >= state.inventory.size() ||
      absolute >= bank_base(state) + kPlayerBankWords)
    return 0;
  return state.inventory[absolute];
}

void InventoryEngine::set_slot(ScenarioState &state, size_t slot_idx,
                               uint32_t value) {
  if (slot_idx == kExtraSlot) { set_extra_slot(state, value); return; }
  const size_t absolute = bank_base(state) + slot_idx + 2;
  if (slot_idx >= get_capacity(state) || absolute >= state.inventory.size() ||
      absolute >= bank_base(state) + kPlayerBankWords)
    return;
  if (value == 0) clear_selector_if_removed(state, absolute);
  state.inventory[absolute] = value;
  if (value != 0 && has_equipped_weapon_behavior(value) &&
      state.inventory[bank_base(state)] == absolute - bank_base(state)) {
    state.equipped_weapon = ItemDatabase::weapon_index(static_cast<uint16_t>(value >> 16));
  }
}

uint32_t InventoryEngine::get_extra_slot(const ScenarioState &state) {
  const size_t absolute = bank_base(state) + 1;
  return absolute < state.inventory.size() ? state.inventory[absolute] : 0;
}

void InventoryEngine::set_extra_slot(ScenarioState &state, uint32_t value) {
  const size_t absolute = bank_base(state) + 1;
  if (absolute >= state.inventory.size()) return;
  if (value == 0) clear_selector_if_removed(state, absolute);
  state.inventory[absolute] = value;
  if (value && state.inventory[bank_base(state)] == 1 &&
      has_equipped_weapon_behavior(value))
    state.equipped_weapon = ItemDatabase::weapon_index(
        static_cast<uint16_t>((value >> 16) & 0xffffu));
}

uint16_t InventoryEngine::get_item_id(uint32_t slot_value) {
  return static_cast<uint16_t>((slot_value >> 16) & 0xffu);
}

uint16_t InventoryEngine::get_quantity(uint32_t slot_value) {
  return static_cast<uint16_t>(slot_value & 0xffffu);
}

uint32_t InventoryEngine::make_slot(uint16_t item_id, uint16_t qty) {
  const uint16_t base_id = item_id & 0xffu;
  if (base_id == 0 || !ItemDatabase::is_valid(base_id)) return 0;
  return encode(base_id, qty, item_id & 0xff00u);
}

std::optional<size_t> InventoryEngine::equipped_slot(const ScenarioState &state) {
  const size_t base = bank_base(state);
  if (base + kPlayerBankWords > state.inventory.size()) return std::nullopt;
  const uint32_t selected_word = state.inventory[base];
  if (selected_word == 1 && has_equipped_weapon_behavior(state.inventory[base + 1]))
    return kExtraSlot;
  if (selected_word < 2 || selected_word >= get_capacity(state) + 2)
    return std::nullopt;
  const uint32_t word = state.inventory[base + selected_word];
  if (!has_equipped_weapon_behavior(word)) return std::nullopt;
  return static_cast<size_t>(selected_word - 2);
}

bool InventoryEngine::is_equipped(const ScenarioState &state, size_t slot_idx) {
  const auto selected = equipped_slot(state);
  return selected && *selected == slot_idx;
}

bool InventoryEngine::equip_weapon(ScenarioState &state, size_t slot_idx,
                                   std::string &message) {
  const uint32_t word = get_slot(state, slot_idx);
  const uint16_t id = get_item_id(word);
  if (!has_equipped_weapon_behavior(word)) {
    message = "Cannot equip this item.";
    return false;
  }
  const auto &info = ItemDatabase::get(id);
  const size_t base = bank_base(state);
  if (base + kPlayerBankWords > state.inventory.size()) {
    message = "Invalid player inventory bank.";
    return false;
  }
  const uint32_t selected = slot_idx == kExtraSlot ? 1u : static_cast<uint32_t>(slot_idx + 2);
  const int32_t selected_weapon = ItemDatabase::weapon_index(
      static_cast<uint16_t>(word >> 16));
  if (state.inventory[base] == selected &&
      state.equipped_weapon == selected_weapon) {
    state.inventory[base] = 0;
    state.equipped_weapon = 0;
    state.flags[11][0] &= ~0x10000000u;
    message = "Weapon unequipped.";
    return true;
  }
  state.inventory[base] = selected;
  state.equipped_weapon = selected_weapon;
  if (id == 131) state.flags[11][0] |= 0x10000000u;
  message = "Equipped " + info.name + ".";
  return true;
}

bool InventoryEngine::equip_extra_item(ScenarioState &state, std::string &message) {
  return equip_weapon(state, kExtraSlot, message);
}

bool InventoryEngine::use_extra_item(ScenarioState &state, std::string &message) {
  return use_item(state, kExtraSlot, message);
}

bool InventoryEngine::consume_equipped_ammo(ScenarioState &state,
                                             uint16_t amount) {
  const auto selected = equipped_slot(state);
  if (!selected || amount == 0) return false;
  const uint32_t word = get_slot(state, *selected);
  const int weapon_index = ItemDatabase::weapon_index(static_cast<uint16_t>(word >> 16));
  if (weapon_index <= 2) // knife and lighter never consume firearm ammunition
    return false;
  if (word & 0x08000000u) return true;
  const uint16_t qty = get_quantity(word);
  if (qty < amount) return false;
  set_slot(state, *selected,
           encode(get_item_id(word), static_cast<uint16_t>(qty - amount),
                  ammo_flag(word)));
  return true;
}

bool InventoryEngine::can_use_item(const ScenarioState &state, size_t slot_idx) {
  const uint32_t word = get_slot(state, slot_idx);
  const uint16_t id = get_item_id(word);
  if (!id || !ItemDatabase::is_valid(id)) return false;
  if (has_equipped_weapon_behavior(word) || ItemDatabase::is_key_item(id) ||
      ItemDatabase::is_ammo(id)) {
    if (!source_use_area_matches(state, id)) return false;
    if (id == 82 && get_quantity(word) == 0 && state.stage == 9 && state.room == 8)
      return false;
    return true;
  }
  if (!get_quantity(word)) return false;
  if (id >= 20 && id <= 29) {
    static constexpr std::array<uint8_t, 10> cure{{4, 1, 0, 16, 2, 4, 17, 18, 3, 20}};
    const uint8_t effect = cure[id - 20];
    const bool heal = (effect & 0x0f) != 0;
    // Use_00 still runs for chronic illness: it clears ordinary poison without
    // consuming the blue herb, so the menu must allow the attempt.
    return heal || ((effect & 0xf0) == 0x10);
  }
  return false;
}

bool InventoryEngine::use_item(ScenarioState &state, size_t slot_idx,
                               std::string &message) {
  const uint32_t word = get_slot(state, slot_idx);
  const uint16_t id = get_item_id(word);
  const uint16_t qty = get_quantity(word);
  if (id == 0 || !ItemDatabase::is_valid(id)) {
    message = "Slot is empty.";
    return false;
  }
  if (has_equipped_weapon_behavior(word) || ItemDatabase::is_key_item(id) ||
      ItemDatabase::is_ammo(id)) {
    if (!source_use_area_matches(state, id)) {
      message = "This item cannot be used here.";
      return false;
    }
    if (id == 82 && qty == 0 && state.stage == 9 && state.room == 8) {
      message = "This item has no remaining uses.";
      return false;
    }
    state.selected_item_id = id;
    state.flags[10][0] |= 0x400u;
    if (id == 4) state.inventory[224] = static_cast<uint8_t>(qty);
    if (id == 82 && qty != 0)
      set_slot(state, slot_idx, qty == 1 ? 0 : encode(id, qty - 1, ammo_flag(word)));
    message = "Used " + ItemDatabase::get_name(id) + ".";
    return true;
  }
  if (qty == 0) { message = "This item has no remaining uses."; return false; }
  if (id >= 20 && id <= 29) {
    static constexpr std::array<uint8_t, 10> cure{{4, 1, 0, 16, 2, 4, 17, 18, 3, 20}};
    const uint8_t effect = cure[id - 20];
    const uint8_t heal = effect & 0x0f;
    const bool cures_poison = (effect & 0xf0) == 0x10;
    const int32_t cap = static_cast<int32_t>(max_health(state));
    if (id == 20) ++state.spray_use_count; // sys->spray_ct increments on Use_00.
    if (heal == 0 && !cures_poison) {
      message = "This item cannot be used by itself.";
      return false;
    }
    if (heal == 0 && cures_poison &&
        (state.player_state_flags & 0x00200000u)) {
      state.player_state_flags &= ~kPoisonFlag;
      message = "The poison subsides, but the condition remains.";
      return false;
    }
    switch (heal) {
    case 1: state.player_health += state.game_mode == 2 ? 100 : 50; break;
    case 2: state.player_health += state.game_mode == 2 ? 200 : 100; break;
    case 3: case 4: state.player_health = cap; break;
    default: break;
    }
    state.player_health = std::min(state.player_health, cap);
    if (cures_poison) state.player_state_flags &= ~kPoisonFlag;
    set_slot(state, slot_idx,
             qty > 1 ? encode(id, qty - 1, ammo_flag(word)) : 0);
    message = "Used " + ItemDatabase::get_name(id) + ". Condition recovered.";
    return true;
  }
  message = "Cannot use this right now.";
  return false;
}

bool InventoryEngine::can_combine(const ScenarioState &state, size_t a,
                                  size_t b) {
  if (a == b) return false;
  const uint32_t wa = get_slot(state, a), wb = get_slot(state, b);
  const uint16_t ia = get_item_id(wa), ib = get_item_id(wb);
  if (!ia || !ib || !ItemDatabase::is_valid(ia) || !ItemDatabase::is_valid(ib))
    return false;
  const uint16_t ra = normalized_recipe_id(wa), rb = normalized_recipe_id(wb);
  return find_recipe(rb, ra).has_value();
}

bool InventoryEngine::combine_slots(ScenarioState &state, size_t a, size_t b,
                                    std::string &message) {
  if (!can_combine(state, a, b)) {
    message = "These items cannot be combined.";
    return false;
  }
  const uint32_t wa = get_slot(state, a), wb = get_slot(state, b);
  auto recipe = find_recipe(normalized_recipe_id(wb), normalized_recipe_id(wa));
  if (!recipe) { message = "These items cannot be combined."; return false; }
  const bool reverse = recipe->mode == 1 || recipe->mode == 4 ||
                       recipe->mode == 5 || recipe->mode == 8 || recipe->mode == 11;
  // Dispatch operands always follow the original selected cursor order; the
  // recipe owner only chooses the table row, not the helper pointer order.
  auto result = combine_original_words(reverse ? wb : wa,
                                       reverse ? wa : wb,
                                       state.game_mode, recipe->mode, recipe->row);
  if (!result.success) { message = "These items cannot be combined."; return false; }
  ScenarioState staged = state;
  const size_t base = bank_base(staged);
  const uint32_t old_selector = staged.inventory[base];
  const int32_t old_weapon = staged.equipped_weapon;
  staged.inventory[base + a + 2] = reverse ? result.target_after : result.source_after;
  staged.inventory[base + b + 2] = reverse ? result.source_after : result.target_after;
  const size_t cap = get_capacity(staged);
  size_t occupied = 0;
  for (size_t i = 0; i < cap; ++i) {
    const uint16_t id = get_item_id(get_slot(staged, i));
    if (id) occupied += std::max<uint8_t>(1, ItemDatabase::get(id).slot_count);
  }
  if (occupied > cap) { message = "There is not enough inventory space."; return false; }
  const uint32_t helper_source = reverse ? wb : wa;
  const uint32_t helper_target = reverse ? wa : wb;
  const size_t helper_target_slot = reverse ? a : b;
  const auto selected = equipped_slot(state);
  const uint32_t selected_word = selected ? get_slot(state, *selected) : 0;
  const bool combi00_rebind = (recipe->mode == 0 || recipe->mode == 1) &&
      get_item_id(helper_source) == 19 && get_item_id(helper_target) == 9 &&
      selected && get_item_id(selected_word) == 9;
  const bool combi02_rebind = (recipe->mode == 3 || recipe->mode == 4) &&
      selected && (*selected == a || *selected == b);
  if (combi00_rebind || combi02_rebind) {
    staged.inventory[base] = old_selector;
    staged.equipped_weapon = old_weapon;
    std::string equip_message;
    if (!equip_weapon(staged, helper_target_slot, equip_message)) {
      message = "The resulting weapon could not be equipped.";
      return false;
    }
  } else {
    const uint32_t selector = staged.inventory[base];
    if (selector >= 1 && selector < kPlayerBankWords) {
      const uint32_t selected_word_after = staged.inventory[base + selector];
      staged.equipped_weapon = has_equipped_weapon_behavior(selected_word_after)
          ? ItemDatabase::weapon_index(static_cast<uint16_t>(selected_word_after >> 16)) : 0;
      if (!has_equipped_weapon_behavior(selected_word_after)) staged.inventory[base] = 0;
    }
  }
  state = std::move(staged);
  message = "Combined the items.";
  return true;
}

bool InventoryEngine::swap_slots(ScenarioState &state, size_t a, size_t b) {
  if (a == b || a >= get_capacity(state) || b >= get_capacity(state)) return false;
  const size_t base = bank_base(state);
  if (base + kPlayerBankWords > state.inventory.size()) return false;
  const uint32_t wa = get_slot(state, a), wb = get_slot(state, b);
  const auto selected = equipped_slot(state);
  set_slot(state, a, wb); set_slot(state, b, wa);
  if (selected && *selected == a) state.inventory[bank_base(state)] = static_cast<uint32_t>(b + 2);
  else if (selected && *selected == b) state.inventory[bank_base(state)] = static_cast<uint32_t>(a + 2);
  const auto final_slot = equipped_slot(state);
  state.equipped_weapon = final_slot
      ? ItemDatabase::weapon_index(static_cast<uint16_t>(get_slot(state, *final_slot) >> 16))
      : 0;
  return true;
}

bool InventoryEngine::add_item(ScenarioState &state, uint16_t id,
                               uint16_t quantity) {
  if (!id || !quantity || !ItemDatabase::is_valid(id)) return false;
  return add_item_word(state, make_slot(id, quantity));
}

bool InventoryEngine::add_item_word(ScenarioState &state, uint32_t raw_word) {
  const uint16_t id = get_item_id(raw_word);
  uint32_t remaining = get_quantity(raw_word);
  if (!id || !ItemDatabase::is_valid(id)) return false;
  const size_t cap = get_capacity(state), base = bank_base(state);
  if (base + kPlayerBankWords > state.inventory.size()) return false;
  if (id == 55) {
    if (get_extra_slot(state) != 0) return false;
    set_extra_slot(state, raw_word);
    return true;
  }
  const auto inventory_before = state.inventory;
  const int weapon_before = state.equipped_weapon;
  const bool zero_quantity_word = remaining == 0;
  const bool source_ammo = (ItemDatabase::get(id).original_attributes & 0x0200u) != 0;
  // Source ItemGet installs each non-stackable grant as one raw word. The
  // ITEM_WORK.max value is not a cap for weapon-loaded quantities (e.g. ID34).
  const uint16_t limit = source_ammo ? 999 : 0xffff;
  if (source_ammo && remaining) {
    for (size_t i = 0; i < cap && remaining; ++i) {
      const uint32_t current_word = get_slot(state, i);
      if (get_item_id(current_word) != id) continue;
      const uint16_t current = get_quantity(current_word);
      // GetItem only considers a reserve word while it is below the source
      // cap. A full infinite word is skipped, allowing a new record.
      if (current >= limit) continue;
      // Retail GetItem consumes a reserve pickup when its existing stack is
      // infinite instead of adding the pickup quantity.
      if (current_word & 0x08000000u) {
        remaining = 0;
        break;
      }
      const uint16_t amount = static_cast<uint16_t>(std::min<uint32_t>(remaining,
          current < limit ? limit - current : 0));
      if (amount) {
        set_slot(state, i, encode(id, current + amount, ammo_flag(current_word)));
        remaining -= amount;
      }
    }
  }
  bool inserted_zero = false;
  while (remaining || (zero_quantity_word && !inserted_zero)) {
    size_t used_slots = 0;
    for (size_t i = 0; i < cap; ++i) {
      const uint16_t present = get_item_id(get_slot(state, i));
      if (present) used_slots += std::max<uint8_t>(1, ItemDatabase::get(present).slot_count);
    }
    const size_t item_slots = std::max<uint8_t>(1, ItemDatabase::get(id).slot_count);
    size_t free_slot = cap;
    for (size_t i = 0; i < cap; ++i) if (get_slot(state, i) == 0) { free_slot = i; break; }
    if (free_slot == cap || used_slots + item_slots > cap) {
      state.inventory = inventory_before;
      state.equipped_weapon = weapon_before;
      return false;
    }
    const uint16_t amount = zero_quantity_word ? 0 :
        static_cast<uint16_t>(std::min<uint32_t>(limit, remaining));
    const uint32_t word = zero_quantity_word ? raw_word :
        encode(id, amount, static_cast<uint16_t>((raw_word >> 16) & 0xff00u));
    set_slot(state, free_slot, word);
    if (zero_quantity_word) inserted_zero = true;
    else remaining -= amount;
  }
  return true;
}

int InventoryEngine::count_item(const ScenarioState &state, uint16_t id) {
  int total = 0;
  for (size_t i = 0; i < get_capacity(state); ++i)
    if (get_item_id(get_slot(state, i)) == id) total += get_quantity(get_slot(state, i));
  return total;
}

bool InventoryEngine::remove_item(ScenarioState &state, uint16_t id,
                                  uint16_t quantity) {
  if (quantity == 0 || count_item(state, id) < quantity) return false;
  uint16_t remaining = quantity;
  for (size_t i = 0; i < get_capacity(state) && remaining; ++i) {
    const uint32_t word = get_slot(state, i);
    if (get_item_id(word) != id) continue;
    const uint16_t current = get_quantity(word);
    if (current <= remaining) { remaining -= current; set_slot(state, i, 0); }
    else { set_slot(state, i, encode(id, current - remaining, ammo_flag(word))); remaining = 0; }
  }
  return true;
}

std::string InventoryEngine::get_condition_string(int32_t hp, bool poisoned) {
  if (poisoned) return "POISON";
  if (hp >= 120) return "FINE";
  if (hp >= 30) return "CAUTION";
  return "DANGER";
}

uint32_t InventoryEngine::get_condition_color(int32_t hp, bool poisoned) {
  if (poisoned) return 0xb040d0;
  if (hp >= 120) return 0x20d040;
  if (hp >= 60) return 0xe0c020;
  if (hp >= 30) return 0xe08020;
  return 0xe03030;
}

} // namespace ashford
