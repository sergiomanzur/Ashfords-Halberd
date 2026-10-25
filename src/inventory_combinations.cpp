#include "inventory_combinations.hpp"

#include "item_database.hpp"

#include <array>

namespace ashford {
namespace {
uint16_t quantity(uint32_t word) { return static_cast<uint16_t>(word); }
uint16_t raw_item(uint32_t word) { return static_cast<uint16_t>(word >> 16); }
uint8_t item_id(uint32_t word) { return static_cast<uint8_t>(word >> 16); }
uint16_t type_flags(uint32_t word) { return raw_item(word) & 0xff00u; }
uint32_t pack(uint16_t id_and_flags, uint16_t qty) {
  return (static_cast<uint32_t>(id_and_flags) << 16) | qty;
}
uint16_t cap_for(uint16_t id, int mode) {
  if (!ItemDatabase::is_valid(id)) return 0;
  return static_cast<uint16_t>(ItemDatabase::get(id).original_stack_limits[
      static_cast<size_t>(mode)]);
}
uint16_t max_for(uint16_t a, uint16_t b) {
  return (a == 42 || b == 42) ? 300 : 999;
}
bool valid_word(uint32_t word) {
  return ItemDatabase::is_valid(item_id(word));
}
} // namespace

OriginalCombinationResult combine_original_words(uint32_t source,
                                                  uint32_t target,
                                                  int game_mode,
                                                  uint8_t opcode,
                                                  size_t row) {
  OriginalCombinationResult out{false, source, target};
  const auto &table = ItemDatabase::original_combinations();
  if (game_mode < 0 || game_mode >= 4 || row > table.size() - 3 ||
      (static_cast<uint16_t>(table[row + 2]) & 0xffu) != opcode ||
      opcode > 11 || !valid_word(source) ||
      !valid_word(target)) return out;
  const uint16_t output = static_cast<uint16_t>(table[row + 1]);
  if (!ItemDatabase::is_valid(output)) return out;

  uint16_t a = quantity(source), b = quantity(target);
  uint16_t ia = item_id(source), ib = item_id(target);
  const uint16_t fa = type_flags(source), fb = type_flags(target);

  switch (opcode) {
  case 0: case 1: { // Combi_00: refill/convert target, returning overflow to source.
    const uint16_t limit = (ia == 42 || ib == 42)
        ? 300 : cap_for(ib, game_mode);
    if (b == limit && ia != 19 && !(fb & 0x800)) return out;
    if (ia != 19) b = static_cast<uint16_t>(b + a); // original unsigned-short wrap
    const uint16_t result_limit = (ia == 42 || output == 42)
        ? 300 : cap_for(output, game_mode);
    if (b > result_limit && !(fb & 0x800)) {
      if (!(fa & 0x800)) a = static_cast<uint16_t>(b - result_limit);
      b = result_limit;
    } else {
      if (fb & 0x800) b = result_limit;
      if (ia != 19 && !(fa & 0x800)) { ia = 0; a = 0; }
    }
    if (ia == 19) { ia = 0; a = 0; }
    out.source_after = pack(static_cast<uint16_t>(ia | (fa & 0x800)), a);
    out.target_after = pack(static_cast<uint16_t>(output | fb), b);
    break;
  }
  case 2: case 5: { // Combi_01: merge and convert, preserving overflow.
    uint16_t total = static_cast<uint16_t>(a + b);
    const uint16_t limit = max_for(ia, ib);
    uint16_t remainder = 0;
    if (total > limit) { remainder = static_cast<uint16_t>(total - limit); total = limit; }
    uint16_t result_flags = static_cast<uint16_t>(output | (fa & 0x800) | (fb & 0x800));
    if (ib == 6) {
      if (ia == 16) result_flags |= 0x1000;
      else if (ia == 17) result_flags |= 0x2000;
      else if (ia == 37) result_flags |= 0x4000;
      switch (fb & 0x7000) {
      case 0x1000: ia = 16; break; case 0x2000: ia = 17; break;
      case 0x4000: ia = 37; break; default: ia = 15; break;
      }
    } else if (ib == 7) {
      if (ia == 36) result_flags |= 0x2000;
      ia = (fb & 0x2000) ? 36 : 18;
    }
    out.source_after = pack(remainder ? ia : 0, remainder);
    out.target_after = pack(result_flags, total);
    break;
  }
  case 3: case 4: { // Combi_02: load grenade launcher/bow ammo; preserve exact powder arithmetic.
    uint16_t flags_a = fa & 0x800, flags_b = fb & 0x800;
    uint16_t result_flags = output;
    bool powder = false;
    bool keep_source_flags = true;
    if (ib == 6) {
      if (ia == 16) result_flags |= 0x1000;
      else if (ia == 17) result_flags |= 0x2000;
      else if (ia == 37) result_flags |= 0x4000;
      switch (fb & 0x7000) {
      case 0x1000: ia = 16; break; case 0x2000: ia = 17; break;
      case 0x4000: ia = 37; break; default: ia = 15; break;
      }
      const uint16_t source_rounds = a;
      a = b;
      b = source_rounds;
    } else if (ib == 7) {
      if (ia == 35 || ia == 130) { powder = true; result_flags |= 0x2000; }
      else if (ia == 36) result_flags |= 0x2000;
      ia = (fb & 0x2000) ? 36 : 18;
      if (powder) {
        const uint16_t source_powder = a;
        const uint16_t target_loaded = b;
        if (!target_loaded) return out;
        if (target_loaded < source_powder) {
          a = static_cast<uint16_t>(source_powder - target_loaded);
          b = target_loaded;
          ia = 35;
          keep_source_flags = false; // Original assignment to itemid1 = 35 clears flags.
        } else {
          a = static_cast<uint16_t>(target_loaded - source_powder);
          b = source_powder;
        }
      } else {
        const uint16_t source_rounds = a;
        a = b;
        b = source_rounds;
      }
    } else return out;
    result_flags = static_cast<uint16_t>(result_flags | flags_b | flags_a);
    if (a == 0) ia = 0;
    out.source_after = pack(static_cast<uint16_t>(ia | (a && keep_source_flags ? flags_a : 0)), a);
    out.target_after = pack(result_flags, b);
    break;
  }
  case 6:
  case 9: {
    const uint16_t amount = cap_for(output, game_mode);
    out.source_after = 0;
    out.target_after = pack(output, amount);
    break;
  }
  case 7: case 8: { // Combi_04: subtract quantities, convert the depleted side.
    if (b <= a) { a = static_cast<uint16_t>(a - b); ib = output; }
    else { b = static_cast<uint16_t>(b - a); ia = output; }
    if (!a) ia = 0;
    if (!b) ib = 0;
    out.source_after = pack(ia, a);
    out.target_after = pack(ib, b);
    break;
  }
  case 10: case 11: { // Combi_05: powder/weapon-part upgrade table.
    static constexpr uint16_t upgrades[4][4] = {
      {122,123,124,66}, {123,124,66,0}, {124,66,0,0}, {66,0,0,0}};
    const uint16_t count = a;
    if (!count || count > 4) return out;
    unsigned group = ib == 79 ? 0 : ib == 122 ? 1 : ib == 123 ? 2 : ib == 124 ? 3 : 4;
    if (group == 4) return out;
    const uint16_t upgraded = upgrades[group][count - 1];
    const uint16_t amount = cap_for(upgraded, game_mode);
    out.source_after = 0;
    out.target_after = pack(upgraded, amount);
    break;
  }
  default: return out;
  }
  out.success = true;
  return out;
}

} // namespace ashford
