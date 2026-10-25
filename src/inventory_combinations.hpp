#pragma once

#include <cstdint>
#include <cstddef>

namespace ashford {

// Portable, scalar adaptation of sub1.c's Combi_00..Combi_05.  combination_row
// is the first (expected-input) word of a three-word combidata row.
struct OriginalCombinationResult {
  bool success = false;
  uint32_t source_after = 0;
  uint32_t target_after = 0;
};

OriginalCombinationResult combine_original_words(uint32_t source,
                                                  uint32_t target,
                                                  int game_mode,
                                                  uint8_t opcode,
                                                  size_t combination_row);

} // namespace ashford
