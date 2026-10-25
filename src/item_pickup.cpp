#include "item_pickup.hpp"

#include "inventory_engine.hpp"
#include "item_database.hpp"
#include "scenario.hpp"
#include "scenario_collision.hpp"

#include <exception>
#include <optional>

namespace ashford {
bool accept_selected_item(ScenarioState &state, std::string &message) {
  const auto fail = [&state, &message](const char *reason) {
    state.flags[10][0] &= ~0x10u;
    message = reason;
    return false;
  };
  if (!(state.flags[10][0] & 0x10u)) {
    message = "No confirmed pickup is pending.";
    return false;
  }
  if (state.interaction_index >= state.interactions.size()) {
    return fail("Pickup trigger is outside the current room.");
  }

  // Resolve both authored links before staging inventory. The current live
  // miscellaneous record is authoritative when collision work is bound.
  const CollisionRecord *trigger_ptr = nullptr;
  try {
    trigger_ptr = &scenario_interaction(state, state.interaction_index);
  } catch (const std::exception &) {
    return fail("Pickup trigger binding is invalid.");
  }
  const auto &trigger = *trigger_ptr;
  if (trigger.type != 4 || !(trigger.flags & 1u)) {
    return fail("Selected interaction is not an active standing pickup.");
  }
  const size_t item_index = trigger.parameters[0];
  if (item_index >= state.items.size()) {
    return fail("Pickup item is outside the current room.");
  }
  const auto &item = state.items[item_index];
  if (!(item.flags & 1u) || (item.state_flags & 0x1000000u) ||
      item.id == 0 || item.id != state.selected_item_id ||
      ItemDatabase::get(item.id).id != item.id) {
    return fail("Selected pickup no longer matches its active room item.");
  }
  if (state.game_mode < 0 || state.game_mode >= 4) {
    return fail("Pickup game mode is outside the authored quantity table.");
  }
  uint32_t authored_grant =
      ItemDatabase::get(item.id).original_stack_limits[state.game_mode];
  const uint32_t roomno = uint32_t(state.stage) * 100u + state.room;
  if (item.id == 31 && roomno == 1)
    authored_grant = 1;
  else if (item.id == 4 && (roomno == 926 || roomno == 706))
    authored_grant = state.inventory[224] & 0xffu;
  // Zero is a source quantity too: keys such as item82 retain their ID even
  // with no remaining uses; Use_05 decides whether the selected area accepts it.

  // GetItem first packs the complete getbulletmax word beside the item ID;
  // high bits carry item variants (for example item 34's 0x800 marker).
  uint32_t grant_word = (uint32_t(item.id) << 16) | authored_grant;
  if ((item.id == 33 || item.id == 34) && state.player_id == 0)
    grant_word &= 0xffff0000u;
  if (item.id == 11)
    grant_word |= 0x08000000u;
  if (state.flags[10][0] & 0x8000u) {
    if (item.id == 9)
      grant_word -= 3;
    else if (item.id == 3)
      grant_word -= 2;
  }

  ScenarioState staged = state;
  if (!InventoryEngine::add_item_word(staged, grant_word)) {
    return fail("Inventory is full.");
  }
  bool overflow_from_existing = false;
  for (size_t slot = 0; slot < InventoryEngine::get_capacity(state); ++slot) {
    const auto word = InventoryEngine::get_slot(state, slot);
    if (InventoryEngine::get_item_id(word) == item.id &&
        InventoryEngine::get_quantity(word) < 999 && !(word & 0x08000000u))
      overflow_from_existing = true;
  }
  if (item.id == 37 && roomno == 551 && !overflow_from_existing) {
    // Retail sets this marker only when it creates a new word. A matching
    // regular stack receives its normal grant; an already-infinite stack
    // consumes the pickup without changing quantity.
    for (size_t slot = 0; slot < InventoryEngine::get_capacity(staged); ++slot) {
      const uint32_t before = InventoryEngine::get_slot(state, slot);
      const uint32_t after = InventoryEngine::get_slot(staged, slot);
      if (!before && InventoryEngine::get_item_id(after) == item.id) {
        InventoryEngine::set_slot(staged, slot, after | 0x08000000u);
        break;
      }
    }
  }
  if ((state.flags[10][0] & 0x8000u) && item.id == 9) {
    std::optional<size_t> granted_slot;
    for (size_t slot = 0; slot < InventoryEngine::get_capacity(staged); ++slot) {
      if (InventoryEngine::get_slot(state, slot) == 0 &&
          InventoryEngine::get_item_id(InventoryEngine::get_slot(staged, slot)) ==
              item.id) {
        granted_slot = slot;
        break;
      }
    }
    if (!granted_slot || !InventoryEngine::equip_weapon(staged, *granted_slot,
                                                         message))
      return fail("Starting handgun could not be selected.");
  }

  // GetItem signals acceptance; the authored event14 supplies the progress
  // counter and keep-trigger choice, then mutates the world on a later tick.
  state.inventory = staged.inventory;
  state.equipped_weapon = staged.equipped_weapon;
  state.flags[10][0] |= 0x800u;
  state.flags[10][0] &= ~(0x10u | 0x8000u);
  message = "Pickup accepted.";
  return true;
}
} // namespace ashford
