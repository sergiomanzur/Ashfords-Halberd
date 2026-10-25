#include "../src/item_database.hpp"
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ashford;

static void require(bool ok, const char *expr, int line) {
  if (!ok) throw std::runtime_error(std::string("check failed at line ") + std::to_string(line) + ": " + expr);
}
#define CHECK(expr) require(static_cast<bool>(expr), #expr, __LINE__)

static std::vector<long long> table_values(const std::string &source, const std::string &symbol) {
  const auto start = source.find(symbol);
  CHECK(start != std::string::npos);
  const auto open = source.find('{', start);
  const auto close = source.find("};", open);
  CHECK(open != std::string::npos && close != std::string::npos);
  const std::string body = source.substr(open, close - open);
  const std::regex number("-?[0-9]+");
  std::vector<long long> values;
  for (std::sregex_iterator i(body.begin(), body.end(), number), end; i != end; ++i)
    values.push_back(std::stoll(i->str()));
  return values;
}

static void compare_source(const std::string &source) {
  const auto itemdata = table_values(source, "itemdata[156]");
  const auto caps = table_values(source, "getbulletmax[156][4]");
  const auto recipes = table_values(source, "combidata[410]");
  CHECK(itemdata.size() == 156 * 3);
  CHECK(caps.size() == 156 * 4);
  CHECK(recipes.size() == 410);
  for (uint16_t id = 0; id < 156; ++id) {
    const auto &item = ItemDatabase::get(id);
    CHECK(ItemDatabase::is_valid(id));
    CHECK(item.id == id);
    CHECK(item.max_stack == itemdata[id * 3]);
    CHECK(item.combination_offset == itemdata[id * 3 + 1]);
    CHECK(item.original_attributes == itemdata[id * 3 + 2]);
    CHECK(item.slot_count == ((item.original_attributes & 0x100) ? 2 : 1));
    for (int mode = 0; mode < 4; ++mode)
      CHECK(item.original_stack_limits[mode] == caps[id * 4 + mode]);
  }
  const auto &raw_combinations = ItemDatabase::original_combinations();
  for (size_t i = 0; i < recipes.size(); ++i) CHECK(raw_combinations[i] == recipes[i]);
}

int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    std::ifstream file(argv[1]);
    CHECK(file.good());
    std::ostringstream buffer; buffer << file.rdbuf();
    const std::string source = buffer.str();
    compare_source(source);

    std::string perturbed = source;
    const auto start = perturbed.find("itemdata[156]");
    const auto row = perturbed.find("{   0,   -1,     0 }", start);
    CHECK(row != std::string::npos);
    perturbed.replace(row, std::string("{   0,   -1,     0 }").size(), "{ 999,   -1,     0 }");
    bool rejected = false;
    try { compare_source(perturbed); } catch (const std::runtime_error &) { rejected = true; }
    CHECK(rejected);

    CHECK(!ItemDatabase::is_valid(156));
    CHECK(ItemDatabase::weapon_index(6 | 0x1000) == 15);
    CHECK(ItemDatabase::weapon_index(6 | 0x2000) == 16);
    CHECK(ItemDatabase::weapon_index(7 | 0x2000) == 19);
    CHECK(ItemDatabase::get(2).sprite_texture == 4);
    CHECK(ItemDatabase::get(2).sprite_cell_width == 68);
    CHECK(ItemDatabase::get(34).slot_count == 2);
    CHECK(ItemDatabase::get(26).heal_amount == 50 && ItemDatabase::get(26).cures_poison);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
