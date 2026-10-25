#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace ashford {

enum class ItemCategory : uint32_t {
  None = 0,
  Weapon = 1,
  Ammo = 2,
  WeaponPart = 4,
  Recovery = 8,
  Supply = 16,
  KeyItem = 64
};

inline ItemCategory operator|(ItemCategory a, ItemCategory b) {
  return static_cast<ItemCategory>(static_cast<uint32_t>(a) |
                                   static_cast<uint32_t>(b));
}

inline bool operator&(ItemCategory a, ItemCategory b) {
  return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct ItemDefinition {
  uint16_t id = 0;
  std::string name;
  std::string description;
  ItemCategory category = ItemCategory::None;
  uint16_t max_stack = 1; // Original ITEM_WORK.max/default quantity, not reserve cap.
  uint16_t heal_amount = 0;
  bool cures_poison = false;
  int16_t equipped_weapon_id = -1; // Maps to runtime weapon index if weapon
  uint16_t compatible_ammo_id = 0;
  // Magazine capacity is not present in ITEM_WORK or WPN_TAB; zero means
  // unknown. Sprite geometry follows ItemBoxIconSet's atlas cell calculation.
  uint16_t magazine_capacity = 0;
  uint8_t slot_count = 1;
  int16_t sprite_index = -1;
  // Source ITEM_WORK.cmb offset into the original combidata table.
  int16_t combination_offset = -1;
  uint16_t original_attributes = 0;
  // Exact getbulletmax[item][game_mode] grant amounts per pickup. The original
  // inventory code applies these as additions and caps stored quantities at 999.
  std::array<uint32_t, 4> original_stack_limits{};
  bool name_decoded = false;
  uint8_t sprite_texture = 0xff;
  uint16_t sprite_cell_x = 0;
  uint16_t sprite_cell_y = 0;
  uint8_t sprite_cell_width = 0;
  uint8_t sprite_cell_height = 0;
};

class ItemDatabase {
public:
  static const ItemDefinition &get(uint16_t item_id);
  static bool is_valid(uint16_t item_id);
  static const std::array<int16_t, 410> &original_combinations();
  static int16_t weapon_index(uint16_t item_id_with_variant);
  static bool is_weapon(uint16_t item_id);
  static bool is_ammo(uint16_t item_id);
  static bool is_recovery(uint16_t item_id);
  static bool is_key_item(uint16_t item_id);
  static std::string get_name(uint16_t item_id);
  static std::string get_description(uint16_t item_id);
};

} // namespace ashford
