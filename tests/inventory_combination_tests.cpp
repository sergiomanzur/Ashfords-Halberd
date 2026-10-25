#include "inventory_combinations.hpp"
#include "item_database.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "inventory combination test failed: " << message << '\n';
    std::exit(1);
  }
}
uint32_t word(uint16_t id, uint16_t quantity, uint16_t flags = 0) {
  return (uint32_t(id | flags) << 16) | quantity;
}
uint16_t id(uint32_t value) { return static_cast<uint16_t>(value >> 16) & 0xff; }
uint16_t qty(uint32_t value) { return static_cast<uint16_t>(value); }

std::string read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream contents;
  contents << file.rdbuf();
  return file ? contents.str() : std::string{};
}

std::string extract_function(const std::string &source, const std::string &name) {
  const std::string signature = "unsigned char " + name + "(";
  const size_t start = source.find(signature);
  if (start == std::string::npos) return {};
  const size_t open = source.find('{', start + signature.size());
  if (open == std::string::npos) return {};
  int depth = 0;
  for (size_t i = open; i < source.size(); ++i) {
    if (source[i] == '{') ++depth;
    else if (source[i] == '}' && --depth == 0)
      return source.substr(start, i - start + 1);
  }
  return {};
}

std::string normalize_source(std::string_view source) {
  std::string normalized;
  for (size_t i = 0; i < source.size();) {
    if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '/') {
      i += 2;
      while (i < source.size() && source[i] != '\n') ++i;
    } else if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '*') {
      i += 2;
      while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/')) ++i;
      i = std::min(i + 2, source.size());
    } else if (std::isspace(static_cast<unsigned char>(source[i]))) {
      ++i;
    } else {
      normalized.push_back(source[i++]);
    }
  }
  return normalized;
}

void verify_source_fixture(int argc, char **argv) {
  check(argc > 1, "CTest must pass upstream sub1.c to the source oracle");
  const std::string retail = read_file(argv[1]);
  const std::string fixture = read_file(__FILE__);
  check(!retail.empty() && !fixture.empty(), "source oracle inputs are readable");
  for (const char *name : {"Combi_00", "Combi_01", "Combi_02", "Combi_03",
                           "Combi_04", "Combi_05"}) {
    check(normalize_source(extract_function(retail, name)) ==
              normalize_source(extract_function(fixture, name)),
          "embedded source oracle body matches upstream " /* dynamic below */);
  }
}

bool find_recipe(uint8_t wanted_opcode, uint16_t &owner, size_t &row,
                 uint16_t &input, uint16_t &output) {
  const auto &table = ashford::ItemDatabase::original_combinations();
  for (uint16_t candidate = 0; candidate < 156; ++candidate) {
    const auto offset = ashford::ItemDatabase::get(candidate).combination_offset;
    if (offset < 0 || static_cast<size_t>(offset) >= table.size()) continue;
    const int count = table[static_cast<size_t>(offset)];
    if (count < 0 || count > 136) continue;
    for (int i = 0; i < count; ++i) {
      const size_t at = static_cast<size_t>(offset + 1 + i * 3);
      if (at + 2 >= table.size() || table[at] < 0 || table[at] >= 156 ||
          table[at + 1] < 0 || table[at + 1] >= 156) continue;
      if ((static_cast<uint16_t>(table[at + 2]) & 0xff) == wanted_opcode) {
        owner = candidate;
        row = at;
        input = static_cast<uint16_t>(table[at]);
        output = static_cast<uint16_t>(table[at + 1]);
        return true;
      }
    }
  }
  return false;
}
bool find_row(uint16_t wanted_input, uint16_t wanted_output, uint8_t opcode,
              size_t &row) {
  const auto &table = ashford::ItemDatabase::original_combinations();
  for (size_t at = 0; at + 2 < table.size(); ++at) {
    if (table[at] == wanted_input && table[at + 1] == wanted_output &&
        (static_cast<uint16_t>(table[at + 2]) & 0xff) == opcode) {
      row = at;
      return true;
    }
  }
  return false;
}
}

// These are the original decompilation helper bodies, extracted verbatim from
// recvx-decomp/src/ps2/veronica/prog/sub1.c (Combi_00..Combi_05).
namespace source_oracle {
struct S_WORK { unsigned int *gb; unsigned int *pip; int listcsr_0, listcsr_1; unsigned int statusflg; };
struct SYS_WORK { int gm_mode; unsigned int st_flg; };
static S_WORK swork{};
static SYS_WORK sys_work{};
static SYS_WORK *sys = &sys_work;
static short combidata[410]{};
static unsigned int getbulletmax[156][4]{};
static void bhSetMessage(int, int) {}
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
unsigned char Combi_00(short ps, unsigned int* moto, unsigned int* aite)
{
    S_WORK* st;
    unsigned short bullet1, bullet2;
    unsigned short itemid1, itemid2;
    unsigned short bullettype0, bullettype;
    unsigned short bltmax;
    unsigned char ok;

    st = &swork;

    ok = 0;

    itemid1 = (*moto >> 16) & 0xFF;
    itemid2 = (*aite >> 16) & 0xFF;

    bullet1 = *moto;
    bullet2 = *aite;

    bullettype0 = (*moto >> 16) & 0xFF00;
    bullettype  = (*aite >> 16) & 0xFF00;

    st->gb = (unsigned int*)&getbulletmax[itemid2][sys->gm_mode];

    if ((itemid1 == 42) || (itemid2 == 42))
    {
        bltmax = 300;
    }
    else
    {
        bltmax = *st->gb;
    }

    if ((bullet2 == bltmax) && (itemid1 != 19) && (!(bullettype & 0x800)))
    {
        if (!(sys->st_flg & 0x200))
        {
            bhSetMessage(1, 156);

            swork.statusflg &= ~0x100000;

            ok = 4;
        }
    }
    else
    {
        if (itemid1 != 19)
        {
            bullet2 += bullet1;
        }

        if ((bullettype & 0x2000))
        {
            itemid2 = combidata[1 + ps];
        }
        else
        {
            itemid2 = combidata[1 + ps];
        }

        st->gb = (unsigned int*)&getbulletmax[itemid2][sys->gm_mode];

        if ((itemid1 == 42) || (itemid2 == 42))
        {
            bltmax = 300;
        }
        else
        {
            bltmax = *st->gb;
        }

        if ((bullet2 > bltmax) && (!(bullettype & 0x800)))
        {
            if (!(bullettype0 & 0x800))
            {
                bullet1 = bullet2 - bltmax;
                bullet2 = bltmax;

                ok = 2;
            }
            else
            {
                bullet2 = bltmax;

                ok = 2;
            }
        }
        else
        {
            if ((bullettype & 0x800))
            {
                bullet2 = bltmax;
            }

            if ((itemid1 != 19) && (!(bullettype0 & 0x800)))
            {
                itemid1 = 0;
                bullet1 = 0;

                ok = 1;
            }
        }
    }

    if (itemid1 == 19)
    {
        itemid1 = 0;
        bullet1 = 0;

        ok = 1;

        if ((unsigned char)(st->pip[*st->pip] >> 16) == 9)
        {
            ok = 129;
        }
    }

    itemid1 |= bullettype0 & 0x800;
    itemid2 |= bullettype;

    *moto = (itemid1 << 16) | bullet1;
    *aite = (itemid2 << 16) | bullet2;

    return ok;
}
unsigned char Combi_01(short ps, unsigned int* moto, unsigned int* aite)
{
    unsigned short bullet1, bullet2;
    unsigned short itemid1, itemid2, itemid3;
    unsigned short bullettype00, bullettype01;
    unsigned short bltmax;
    unsigned char ok;

    itemid1 = (*moto >> 16) & 0xFF;
    itemid2 = (*aite >> 16) & 0xFF;
    itemid3 = 0;

    bullet1 = *moto;
    bullet2 = *aite;

    bullettype00 = (*moto >> 16) & 0xFF00;
    bullettype01 = (*aite >> 16) & 0xFF00;

    if ((itemid1 == 42) || (itemid2 == 42))
    {
        bltmax = 300;
    }
    else
    {
        bltmax = 999;
    }

    if (itemid2 == 6)
    {
        switch (itemid1)
        {
        case 16:
            itemid3 = 4096;
            break;
        case 17:
            itemid3 = 8192;
            break;
        case 37:
            itemid3 = 16384;
            break;
        }

        switch (bullettype01 & 0x7000)
        {
        case 0x1000:
            itemid1 = 16;
            break;
        case 0x2000:
            itemid1 = 17;
            break;
        case 0x4000:
            itemid1 = 37;
            break;
        default:
            itemid1 = 15;
            break;
        }
    }
    else if (itemid2 == 7)
    {
        switch (itemid1)
        {
        case 36:
            itemid3 = 8192;
            break;
        }

        switch (bullettype01 & 0x2000)
        {
        case 0x2000:
            itemid1 = 36;
            break;
        default:
            itemid1 = 18;
            break;
        }
    }

    bullet2 += bullet1;

    itemid3 = ((itemid3 | combidata[ps + 1]) | (bullettype00 & 0x800)) | (bullettype01 & 0x800);

    if (bltmax < bullet2)
    {
        bullet1 = bullet2 - bltmax;
        bullet2 = bltmax;

        ok = 3;
    }
    else
    {
        itemid1 = 0;
        bullet1 = 0;

        ok = 1;
    }

    *moto = (itemid1 << 16) | bullet1;
    *aite = (itemid3 << 16) | bullet2;

    return ok;
}
unsigned char Combi_02(short ps, unsigned int* moto, unsigned int* aite)
{
    unsigned short bullet1, bullet2, bullet3;
    unsigned short itemid1, itemid2, itemid3;
    unsigned short bullettype, bullettype0;
    unsigned short kayaku;
    unsigned char ok;
    unsigned char sine;

    kayaku = 0;

    ok = 0;

    sine = 0;

    itemid1 = (*moto >> 16) & 0xFF;
    itemid2 = (*aite >> 16) & 0xFF;
    itemid3 = 0;

    bullet1 = *moto;
    bullet2 = *aite;

    bullettype0 = (*moto >> 16) & 0xFF00;
    bullettype  = (*aite >> 16) & 0xFF00;

    if (itemid2 == 6)
    {
        if ((*swork.pip == swork.listcsr_0) || (*swork.pip == swork.listcsr_1))
        {
            ok = 128;
        }

        switch (itemid1)
        {
        case 16:
            itemid3 = 4096;
            break;
        case 17:
            itemid3 = 8192;
            break;
        case 37:
            itemid3 = 16384;
            break;
        }

        switch (bullettype & 0x7000)
        {
        case 0x1000:
            itemid1 = 16;
            break;
        case 0x2000:
            itemid1 = 17;
            break;
        case 0x4000:
            itemid1 = 37;
            break;
        default:
            itemid1 = 15;
            break;
        }
    }
    else if (itemid2 == 7)
    {
        if ((*swork.pip == swork.listcsr_0) || (*swork.pip == swork.listcsr_1))
        {
            ok = 128;
        }

        switch (itemid1)
        {
        case 35:
        case 130:
            kayaku = 1;
            itemid3 = 8192;
            break;
        case 36:
            itemid3 = 8192;
            break;
        }

        switch (bullettype & 0x2000)
        {
        case 0x2000:
            itemid1 = 36;
            break;
        default:
            itemid1 = 18;
            break;
        }
    }

    itemid1 |= bullettype0 & 0x800;

    if ((itemid2 == 7) && (kayaku) && (!bullet2))
    {
        ok = 0;

        sine = 1;
    }

    if (!sine)
    {
        itemid3 = ((itemid3 | combidata[ps + 1]) | (bullettype & 0x800)) | (bullettype0 & 0x800);

        if (kayaku)
        {
            bullet3 = bullet1;

            if (bullet2 < bullet3)
            {
                bullet1 = bullet1 - bullet2;
                itemid1 = 35;
            }
            else
            {
                bullet1 = bullet2 - bullet1;
                bullet2 = bullet3;
            }
        }
        else
        {
            bullet3 = bullet1;
            bullet1 = bullet2;
            bullet2 = bullet3;
        }

        if (bullet1 == 0)
        {
            itemid1 = 0;

            ok |= 0x1;
        }
        else
        {
            ok |= 0x3;
        }

        *moto = (itemid1 << 16) | bullet1;
        *aite = (itemid3 << 16) | bullet2;
    }

    return ok;
}
unsigned char Combi_03(short ps, unsigned int* moto, unsigned int* aite)
{
    unsigned short bullet2;
    unsigned short tmp; // not from DWARF

    bullet2 = (&combidata[1])[ps];

    swork.gb = (unsigned int*)&getbulletmax[bullet2][sys->gm_mode];

    tmp = *(unsigned short*)swork.gb;

    *moto = 0;
    *aite = (bullet2 << 16) | tmp;

    return 1;
}
unsigned char Combi_04(short ps, unsigned int* moto, unsigned int* aite)
{
    unsigned short bullet1, bullet2;
    unsigned short itemid1, itemid2;
    unsigned char ok;

    ok = 0;

    bullet1 = *moto;
    bullet2 = *aite;

    itemid1 = (*moto >> 16) & 0xFF;
    itemid2 = (*aite >> 16) & 0xFF;

    if (bullet2 <= bullet1)
    {
        bullet1 -= bullet2;

        itemid2 = combidata[ps + 1];

        ok = 128;
    }
    else
    {
        itemid1 = combidata[ps + 1];

        bullet2 -= bullet1;
    }

    if (!bullet1)
    {
        itemid1 = 0;

        ok |= 0x1;
    }

    if (!bullet2)
    {
        itemid2 = 0;

        ok |= 0x2;
    }

    *moto = (itemid1 << 16) | bullet1;
    *aite = (itemid2 << 16) | bullet2;

    return ok;
}
unsigned char Combi_05(short ps, unsigned int* moto, unsigned int* aite) // first parameter not present on DWARF
{
    S_WORK* st = &swork;
    unsigned short dbwtbl[4][4] =
    {
        { 122, 123, 124, 66 },
        { 123, 124,  66,  0 },
        { 124,  66,   0,  0 },
        {  66,   0,   0,  0 }
    };
    unsigned short bullet1, bullet2;
    unsigned short chgid;

    bullet1 = *moto;

    chgid = 0;

    switch ((unsigned char)(*aite >> 16))
    {
    case 79:
        chgid = 0;
        break;
    case 122:
        chgid = 1;
        break;
    case 123:
        chgid = 2;
        break;
    case 124:
        chgid = 3;
        break;
    }

    bullet2 = dbwtbl[chgid][bullet1 - 1];

    st->gb = (unsigned int*)&getbulletmax[bullet2][sys->gm_mode];

    bullet1 = *st->gb;

    *moto = 0;
    *aite = (bullet2 << 16) | bullet1;

    return 1;
}

static void initialize(int mode) {
  sys->gm_mode = mode;
  const auto &table = ashford::ItemDatabase::original_combinations();
  for (size_t i = 0; i < table.size(); ++i) combidata[i] = table[i];
  for (size_t id = 0; id < 156; ++id)
    for (size_t gm = 0; gm < 4; ++gm)
      getbulletmax[id][gm] = ashford::ItemDatabase::get(static_cast<uint16_t>(id)).original_stack_limits[gm];
  static unsigned int inventory_words[16]{};
  inventory_words[0] = 1;
  inventory_words[1] = 0;
  swork = {};
  swork.pip = inventory_words;
}
static uint8_t invoke(uint32_t *first, uint32_t *second, uint8_t opcode, size_t row) {
  switch (opcode) {
  case 0: return Combi_00(static_cast<short>(row), first, second);
  case 1: return Combi_00(static_cast<short>(row), first, second);
  case 2: return Combi_01(static_cast<short>(row), first, second);
  case 3: return Combi_02(static_cast<short>(row), first, second);
  case 4: return Combi_02(static_cast<short>(row), first, second);
  case 5: return Combi_01(static_cast<short>(row), first, second);
  case 6: case 9: return Combi_03(static_cast<short>(row), first, second);
  case 7: return Combi_04(static_cast<short>(row), first, second);
  case 8: return Combi_04(static_cast<short>(row), first, second);
  case 10: return Combi_05(static_cast<short>(row), first, second);
  case 11: return Combi_05(static_cast<short>(row), first, second);
  default: return 0;
  }
}
} // namespace source_oracle
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static ashford::OriginalCombinationResult compare_with_source(
    uint32_t source, uint32_t target, int mode, uint8_t opcode, size_t row) {
  auto result = ashford::combine_original_words(source, target, mode, opcode, row);
  if (mode < 0 || mode >= 4 || opcode > 11 || row >
          ashford::ItemDatabase::original_combinations().size() - 3 ||
      id(source) >= 156 || id(target) >= 156) return result;
  source_oracle::initialize(mode >= 0 && mode < 4 ? mode : 0);
  uint32_t oracle_source = source, oracle_target = target;
  const uint8_t status = source_oracle::invoke(&oracle_source, &oracle_target, opcode, row);
  const bool oracle_success = (opcode == 0 || opcode == 1) ? status != 4 :
      (opcode == 3 || opcode == 4) ? status != 0 : true;
  if (result.success != oracle_success || result.source_after != oracle_source ||
      result.target_after != oracle_target) {
    std::cerr << "source oracle mismatch for opcode " << unsigned(opcode)
              << " at row " << row << " inputs " << std::hex << source << "," << target
              << " module " << result.source_after << "," << result.target_after
              << " source " << oracle_source << "," << oracle_target << std::dec
              << " success " << result.success << "/" << oracle_success << '\n';
    std::exit(1);
  }
  return result;
}

static uint32_t source_word_id(uint16_t id) {
  if (id >= 152 && id <= 154) return 6u << 16;
  if (id == 155) return 7u << 16;
  return uint32_t(id) << 16;
}

static std::vector<uint16_t> item_flag_options(uint16_t normalized_id) {
  uint16_t required_variant = 0;
  if (normalized_id >= 152 && normalized_id <= 154) {
    required_variant = static_cast<uint16_t>(0x1000u << (normalized_id - 152));
  } else if (normalized_id == 155) {
    required_variant = 0x2000;
  }
  std::vector<uint16_t> variants{required_variant};
  std::vector<uint16_t> flags;
  for (uint16_t variant : variants) {
    flags.push_back(variant);
    flags.push_back(static_cast<uint16_t>(variant | 0x0800));
  }
  return flags;
}

static void compare_recipe_corpus() {
  const auto &table = ashford::ItemDatabase::original_combinations();
  constexpr uint16_t quantities[] = {0, 1, 2, 20, 998};
  size_t comparisons = 0;
  for (uint16_t owner = 1; owner < 156; ++owner) {
    const int offset = ashford::ItemDatabase::get(owner).combination_offset;
    if (offset < 0 || static_cast<size_t>(offset) >= table.size()) continue;
    const int count = table[static_cast<size_t>(offset)];
    if (count < 0 || count > 136) continue;
    for (int recipe_index = 0; recipe_index < count; ++recipe_index) {
      const size_t row = static_cast<size_t>(offset + 1 + recipe_index * 3);
      if (row + 2 >= table.size() || table[row] <= 0 || table[row] >= 156 ||
          table[row + 1] < 0 || table[row + 1] >= 156) continue;
      const uint8_t opcode = static_cast<uint8_t>(table[row + 2]);
      if (opcode > 11) continue;
      const bool reverse = opcode == 1 || opcode == 4 || opcode == 5 ||
                           opcode == 8 || opcode == 11;
      uint32_t expected_source = source_word_id(static_cast<uint16_t>(table[row]));
      uint32_t expected_target = source_word_id(owner);
      uint32_t helper_source = reverse ? expected_target : expected_source;
      uint32_t helper_target = reverse ? expected_source : expected_target;
      std::vector<uint16_t> source_flags = item_flag_options(
          reverse ? owner : static_cast<uint16_t>(table[row]));
      std::vector<uint16_t> target_flags = item_flag_options(
          reverse ? static_cast<uint16_t>(table[row]) : owner);
      for (int mode = 0; mode < 4; ++mode) {
        for (uint16_t source_qty : quantities) {
          if ((opcode == 10 || opcode == 11) &&
              (source_qty == 0 || source_qty > 4)) continue;
          for (uint16_t target_qty : quantities) {
            for (uint16_t source_flag : source_flags) {
              for (uint16_t target_flag : target_flags) {
                const uint32_t a = helper_source | (uint32_t(source_flag) << 16) | source_qty;
                const uint32_t b = helper_target | (uint32_t(target_flag) << 16) | target_qty;
                (void)compare_with_source(a, b, mode, opcode, row);
                ++comparisons;
              }
            }
          }
        }
      }
    }
  }
  check(comparisons > 1000, "source oracle covers the recipe and quantity matrix");
}

int main(int argc, char **argv) {
  verify_source_fixture(argc, argv);
  compare_recipe_corpus();
  using ashford::combine_original_words;
  // Item 2's sole recipe is source item 42 -> output item 2, Combi_00.
  auto refill = compare_with_source(word(42, 50), word(2, 290), 0, 0, 9);
  check(refill.success && id(refill.source_after) == 42 && qty(refill.source_after) == 40,
        "Combi_00 returns overflow to the source stack");
  check(id(refill.target_after) == 2 && qty(refill.target_after) == 300,
        "Combi_00 transforms and caps the target");

  uint16_t owner = 0, input = 0, output = 0;
  size_t row = 0;
  check(find_recipe(1, owner, row, input, output), "database has a reversed Combi_00 recipe");
  const auto reverse_refill = compare_with_source(word(owner, 1), word(input, 0), 0, 1, row);
  check(reverse_refill.success && id(reverse_refill.target_after) == output,
        "reversed Combi_00 preserves the dispatch operand order");

  auto infinite = compare_with_source(word(42, 50, 0x0800), word(2, 1), 0, 0, 9);
  check(infinite.success && infinite.source_after == word(42, 50, 0x0800),
        "Combi_00 preserves an infinite source stack");
  check(id(infinite.target_after) == 2 && qty(infinite.target_after) == 51,
        "Combi_00 uses the exact loaded quantity for an infinite source");
  const auto infinite_target = compare_with_source(word(42, 50), word(2, 1, 0x0800), 0, 0, 9);
  check(infinite_target.success && infinite_target.source_after == 0 &&
            qty(infinite_target.target_after) == 300 &&
            (infinite_target.target_after >> 16 & 0x0800),
        "Combi_00 preserves the infinite target flag and fills its capacity");
  const auto infinite_overflow = compare_with_source(word(42, 50, 0x0800), word(2, 290), 0, 0, 9);
  check(infinite_overflow.success && infinite_overflow.source_after == word(42, 50, 0x0800) &&
            qty(infinite_overflow.target_after) == 300,
        "Combi_00 leaves an infinite source quantity intact when target overflows");

  // The key item is consumed, but Combi_00 does not add its quantity.
  const auto key_recipe = compare_with_source(word(19, 1), word(9, 3), 0, 0, 48);
  check(key_recipe.success && key_recipe.source_after == 0 &&
            id(key_recipe.target_after) == 10 && qty(key_recipe.target_after) == 3,
        "Combi_00 consumes item 19 without adding its quantity");

  check(find_recipe(2, owner, row, input, output), "database has a Combi_01 recipe");
  const auto merged = compare_with_source(word(input, 8), word(owner, 7), 0, 2, row);
  check(merged.success && id(merged.source_after) == 0 && id(merged.target_after) == output,
        "Combi_01 merges and converts its recipe operands");
  check(qty(merged.source_after) == 0 && qty(merged.target_after) == 15,
        "Combi_01 preserves the combined quantity below its reserve cap");

  check(find_recipe(5, owner, row, input, output), "database has a reversed Combi_01 recipe");
  const auto reverse_merge = compare_with_source(word(owner, 8), word(input, 7), 0, 5, row);
  check(reverse_merge.success && id(reverse_merge.target_after) == output &&
            qty(reverse_merge.source_after) + qty(reverse_merge.target_after) == 15,
        "reversed Combi_01 merges the same quantities");

  check(find_recipe(3, owner, row, input, output), "database has a Combi_02 recipe");
  const auto converted = compare_with_source(word(input, 6), word(owner, 2), 0, 3, row);
  check(converted.success && id(converted.target_after) == output && qty(converted.target_after) == 6,
        "Combi_02 converts loaded ammunition and transfers the source quantity");

  check(find_row(35, 7, 3, row), "database has a powder-to-bow Combi_02 recipe");
  const auto powder_remaining = compare_with_source(word(35, 5), word(7, 3), 0, 3, row);
  check(powder_remaining.success && id(powder_remaining.source_after) == 35 &&
            qty(powder_remaining.source_after) == 2 &&
            (powder_remaining.target_after >> 16 & 0x7000) == 0x2000 &&
            qty(powder_remaining.target_after) == 3,
        "Combi_02 spends bow arrows before returning excess powder");
  const auto infinite_powder = compare_with_source(word(35, 5, 0x0800), word(7, 3), 0, 3, row);
  check(infinite_powder.success && id(infinite_powder.source_after) == 35 &&
            qty(infinite_powder.source_after) == 2 &&
            (infinite_powder.source_after >> 16 & 0x0800) == 0 &&
            (infinite_powder.target_after >> 16 & 0x0800) != 0,
        "Combi_02 matches the source flag overwrite when powder remains");
  const auto powder_used = compare_with_source(word(35, 2), word(7, 5), 0, 3, row);
  check(powder_used.success && id(powder_used.source_after) == 18 &&
            qty(powder_used.source_after) == 3 &&
            (powder_used.target_after >> 16 & 0x7000) == 0x2000 &&
            qty(powder_used.target_after) == 2,
        "Combi_02 converts the remaining bow arrows and spends the powder");
  const auto empty_bow = compare_with_source(word(35, 2), word(7, 0), 0, 3, row);
  check(!empty_bow.success && empty_bow.source_after == word(35, 2),
        "Combi_02 refuses powder when the bow has no loaded arrows");

  check(find_recipe(6, owner, row, input, output), "database has a Combi_03 recipe");
  const auto made = compare_with_source(word(input, 1), word(owner, 1), 1, 6, row);
  check(made.success && made.source_after == 0 && id(made.target_after) == output &&
            qty(made.target_after) == ashford::ItemDatabase::get(output).original_stack_limits[1],
        "Combi_03 creates the output at the selected game's quantity");

  check(find_recipe(7, owner, row, input, output), "database has a Combi_04 recipe");
  const auto subtracted = compare_with_source(word(input, 9), word(owner, 4), 0, 7, row);
  check(subtracted.success && id(subtracted.source_after) == input &&
            qty(subtracted.source_after) == 5 && id(subtracted.target_after) == output &&
            qty(subtracted.target_after) == 4,
        "Combi_04 consumes matching quantities and converts the depleted side");

  // Item 120 applied to the item-122 weapon follows the source's dbwtbl row.
  check(find_recipe(10, owner, row, input, output), "database has a Combi_05 recipe");
  const auto upgraded = compare_with_source(word(120, 1), word(122, 1), 0, 10, row);
  check(upgraded.success && upgraded.source_after == 0 && id(upgraded.target_after) == 123,
        "Combi_05 applies the source upgrade matrix");

  check(find_recipe(11, owner, row, input, output), "database has a reversed Combi_05 recipe");
  const auto reverse_upgrade = compare_with_source(word(owner, 1), word(input, 1), 0, 11, row);
  check(reverse_upgrade.success && reverse_upgrade.source_after == 0 &&
            id(reverse_upgrade.target_after) == output,
        "reversed Combi_05 applies the matching weapon-part upgrade");

  const auto malformed = compare_with_source(word(42, 1), word(42, 1), 4, 0, 9);
  check(!malformed.success && malformed.source_after == word(42, 1),
        "malformed game modes fail without mutating inputs");
  const auto malformed_row = compare_with_source(word(42, 1), word(2, 1), 0, 0,
                                                     static_cast<size_t>(-1));
  check(!malformed_row.success && malformed_row.target_after == word(2, 1),
        "malformed recipe offsets fail without overflowing bounds checks");
  std::cout << "inventory combination scalar tests passed\n";
}

