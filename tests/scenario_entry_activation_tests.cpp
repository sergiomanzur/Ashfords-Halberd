#include "effects.hpp"
#include "scenario.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
void ck(bool value, const char *why) {
  if (!value)
    throw std::runtime_error(why);
}
template <class F> void bad(F action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Required malformed activation was accepted");
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
void event(ScenarioState &s, const Bytes &code) {
  Bytes data(1214 + code.size());
  put(data, 0, 0x41200000);
  put(data, 16, 128);
  put(data, 28, 1200);
  put(data, 1200, 12);
  put(data, 1204, 13);
  put(data, 1208, 14);
  std::copy(code.begin(), code.end(), data.begin() + 1214);
  s.scenario = std::make_shared<const RoomScenario>(decode_room_scenario(data));
  s.flags[6][0] = 0x10;
  start_scenario_event(s, 0, 0);
  tick_scenario_tasks(s);
}
void common_flags() {
  struct Case {
    uint8_t variant;
    uint32_t initial, want;
  };
  const Case cases[] = {
      {0, 0x81000119, 0x81000001}, {1, 0x80000001, 0x80000119},
      {2, 0x81000119, 0x81000101}, {3, 0x80000001, 0x80000019},
      {4, 0x81000119, 0x81000119}, {255, 0x81000119, 0x81000119}};
  for (const auto &c : cases)
    for (uint8_t group : {1, 2, 3}) {
      ScenarioState s;
      s.enemies.resize(3);
      s.enemy_slots = {2};
      s.objects.resize(1);
      s.items.resize(1);
      auto &w = group == 1   ? s.enemies[2]
                : group == 2 ? s.objects[0]
                             : s.items[0];
      w.id = 1;
      w.flags = c.initial;
      w.state_flags = 0x4000;
      w.modes = {2, 3, 4, 5};
      event(s, Bytes{101, group, 0, 0, 105, 23, c.variant, 255, 55, 6, 0});
      ck(w.flags == c.want && w.state_flags == 0x4000 &&
             w.modes == std::array<uint8_t, 4>{2, 3, 4, 5} &&
             s.enemies[0].flags == 0 && s.enemies[1].flags == 0 &&
             s.room_case == 6 && s.tasks[0].pc == 10,
         "Common23 must change only bound work flags and advance four bytes");
    }
  ScenarioState s;
  s.enemies.resize(1);
  s.enemies[0].flags = 0x119;
  s.tasks[0].work_group = 1;
  s.tasks[0].work_index = 0;
  for (const auto &code : {Bytes{105}, Bytes{105, 23}, Bytes{105, 23, 0}})
    bad([&] { event(s, code); });
  ck(s.enemies[0].flags == 0x119,
     "Truncated common23 must preserve actor flags");
  s.tasks[0].work_index.reset();
  event(s, Bytes{105, 23, 255, 0, 55, 4, 0});
  ck(s.room_case == 4 && s.enemies[0].flags == 0x119,
     "Unmatched common23 must not resolve absent work");
  bad([&] { event(s, Bytes{105, 23, 1, 0, 0}); });
  s.tasks[0].work_index = 1;
  bad([&] { event(s, Bytes{105, 23, 1, 0, 0}); });
  ck(s.enemies[0].flags == 0x119, "Bad bound work must preserve actor flags");
}
void calculation_flags() {
  struct Case {
    uint16_t value;
    uint32_t initial, want;
  };
  const Case cases[] = {{0, 0x80000139, 0x80000119},
                        {1, 0x80000119, 0x80000139},
                        {256, 0x80000119, 0x80000139},
                        {65535, 0x80000119, 0x80000139}};
  for (const auto &c : cases)
    for (uint8_t group : {1, 2, 3}) {
      ScenarioState s;
      s.enemies.resize(3);
      s.enemy_slots = {2};
      s.objects.resize(1);
      s.items.resize(1);
      auto &w = group == 1   ? s.enemies[2]
                : group == 2 ? s.objects[0]
                             : s.items[0];
      w.flags = c.initial;
      w.modes = {2, 3, 4, 5};
      w.state_flags = 0x8000;
      event(s, Bytes{101, group, 0, 0, 105, 46, uint8_t(c.value),
                     uint8_t(c.value >> 8), 55, 3, 0});
      ck(w.flags == c.want && w.state_flags == 0x8000 &&
             w.modes == std::array<uint8_t, 4>{2, 3, 4, 5} &&
             s.enemies[0].flags == 0 && s.enemies[1].flags == 0 &&
             s.room_case == 3 && s.tasks[0].pc == 10,
         "Common46 must toggle only bound work bit20 from full16-bit value");
    }
  ScenarioState s;
  s.enemies.resize(1);
  s.enemies[0].flags = 0x139;
  s.tasks[0].work_group = 1;
  s.tasks[0].work_index = 0;
  for (const auto &code : {Bytes{105, 46}, Bytes{105, 46, 0}})
    bad([&] { event(s, code); });
  ck(s.enemies[0].flags == 0x139, "Truncated common46 must preserve flags");
  s.tasks[0].work_index.reset();
  bad([&] { event(s, Bytes{105, 46, 1, 0, 0}); });
  s.tasks[0].work_index = 1;
  bad([&] { event(s, Bytes{105, 46, 1, 0, 0}); });
  ck(s.enemies[0].flags == 0x139, "Bad common46 work must preserve flags");
}
void defeat() {
  ScenarioState s;
  s.enemies.resize(3);
  s.enemy_slots = {2};
  s.enemies[2].flags = 0x81;
  s.enemies[2].state_flags = 0x4000;
  s.flags[3][1] = 0x80000000;
  run_scenario(Bytes{34, 0, 32, 0, 55, 7, 0}, s);
  ck(s.enemies[2].state_flags == 0x1004000 && s.enemies[2].flags == 0x81 &&
         s.enemies[0].state_flags == 0 && s.enemies[1].state_flags == 0 &&
         s.flags[3][1] == 0x80000000 && s.room_case == 7,
     "Defeat check must hide only authored mapped work and advance four bytes");
  struct Case {
    uint16_t count;
    size_t word;
    uint32_t bit;
  };
  const Case cases[] = {{0, 0, 0x80000000},   {31, 0, 1},
                        {32, 1, 0x80000000},  {511, 15, 1},
                        {512, 0, 0x80000000}, {65535, 15, 1}};
  for (const auto &c : cases) {
    ScenarioState v;
    v.enemies.resize(1);
    v.enemy_slots = {0};
    v.flags[3][c.word] = c.bit;
    const Bytes code{34, 0, uint8_t(c.count), uint8_t(c.count >> 8), 0};
    run_scenario(code, v);
    ck(v.enemies[0].state_flags == 0x1000000,
       "Source defeat bits must be MSB-first and wrap at512");
    v.enemies[0].state_flags = 0x8000;
    v.flags[3][c.word] = c.bit == 1 ? 0x80000000 : 1;
    run_scenario(code, v);
    ck(v.enemies[0].state_flags == 0x8000,
       "Neighbor defeat bit must not hide enemy");
  }
  s.flags[3][1] = 0;
  run_scenario(Bytes{34, 0, 32, 0, 0}, s);
  ck(s.enemies[2].state_flags == 0x1004000,
     "Clear defeat bit must not restore hidden work");
  ScenarioState empty;
  run_scenario(Bytes{34, 255, 0, 0, 55, 8, 0}, empty);
  ck(empty.room_case == 8, "Clear defeat bit must not resolve missing map");
  empty.flags[3][0] = 0x80000000;
  bad([&] { run_scenario(Bytes{34, 255, 0, 0, 0}, empty); });
  s.flags[3][0] = 0x80000000;
  s.enemy_slots[0] = 3;
  bad([&] { run_scenario(Bytes{34, 0, 0, 0, 0}, s); });
  for (const auto &code : {Bytes{34}, Bytes{34, 0}, Bytes{34, 0, 0}})
    bad([&] { run_scenario(code, s); });
  ck(s.enemies[2].state_flags == 0x1004000 && s.enemies[2].flags == 0x81,
     "Rejected defeat check must preserve work");
}
void mode() {
  ScenarioState s;
  auto prior = std::make_shared<NativeEffectPool>();
  prior->works[31].modes = {2, 7, 3, 4};
  prior->works[31].flags = 0; // Source assignment also reaches inactive work.
  prior->works[31].state_flags = 0x1000000;
  prior->works[4].modes = {9, 8, 7, 6};
  s.effect_pool = prior;
  s.effect_slots = {4, 31, 31};
  run_scenario(Bytes{145, 1, 255, 99, 55, 9, 0}, s);
  ck(s.room_case == 9 && s.effect_pool != prior &&
         s.effect_pool->works[31].modes ==
             std::array<uint8_t, 4>{2, 255, 3, 4} &&
         prior->works[31].modes == std::array<uint8_t, 4>{2, 7, 3, 4} &&
         s.effect_pool->works[4].modes == std::array<uint8_t, 4>{9, 8, 7, 6} &&
         s.effect_pool->works[31].flags == 0 &&
         s.effect_pool->works[31].state_flags == 0x1000000,
     "Effect mode must use authored map, preserve snapshots and other fields");
  run_scenario(Bytes{145, 2, 0, 0, 0}, s);
  ck(s.effect_pool->works[31].modes[1] == 0,
     "Aliased authored effect must address the same slot");
  const auto stable = s.effect_pool;
  run_scenario(Bytes{145, 1, 0, 0, 0}, s);
  ck(s.effect_pool->works[31].modes == std::array<uint8_t, 4>{2, 0, 3, 4},
     "Repeated mode must retain other mode bytes");
  bad([&] { run_scenario(Bytes{145, 3, 1, 0, 0}, s); });
  s.effect_slots[0] = 512;
  bad([&] { run_scenario(Bytes{145, 0, 1, 0, 0}, s); });
  for (const auto &code : {Bytes{145}, Bytes{145, 1}, Bytes{145, 1, 1}})
    bad([&] { run_scenario(code, s); });
  ck(s.effect_pool == stable && stable->works[31].modes[1] == 0 &&
         prior->works[31].modes[1] == 7,
     "Invalid effect activation must retain pool snapshots");
  ScenarioState empty;
  empty.effect_slots = {0};
  bad([&] { run_scenario(Bytes{145, 0, 1, 0, 0}, empty); });
  auto full = std::make_shared<NativeEffectPool>();
  for (auto &w : full->works)
    w.flags = 3;
  empty.effect_pool = full;
  empty.effect_slots = {511};
  run_scenario(Bytes{145, 0, 5, 0, 0}, empty);
  ck(empty.effect_pool->works[511].modes[1] == 5 &&
         full->works[511].modes[1] == 0,
     "Mode assignment must not require a free allocation");
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Missing activation test mode");
    if (std::string(argv[1]) == "defeat")
      defeat();
    else if (std::string(argv[1]) == "mode")
      mode();
    else if (std::string(argv[1]) == "common")
      common_flags();
    else if (std::string(argv[1]) == "calculation")
      calculation_flags();
    else
      throw std::runtime_error("Invalid activation test mode");
    std::cout << "Native room activation checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
