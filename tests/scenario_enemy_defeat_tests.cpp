#include "enemy_defeat_reference_fixture.hpp"
#include "scenario.hpp"
#include <iostream>
#include <stdexcept>
using namespace ashford;
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void rejected(F action) {
  bool failed = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  require(failed, "Malformed original defeat reconciliation was accepted");
}
Bytes program(uint8_t authored, uint16_t progress) {
  return {13, authored, uint8_t(progress), uint8_t(progress >> 8), 55, 7, 0};
}
void reconciliation() {
  for (uint16_t progress :
       {uint16_t(0), uint16_t(31), uint16_t(32), uint16_t(511), uint16_t(512),
        uint16_t(1023), uint16_t(0xffff), uint16_t(0x4401)}) {
    for (uint32_t flags : {0u, 1u, 2u, 3u, 0x80000000u, 0x80000003u}) {
      ScenarioState s;
      s.enemies.resize(4);
      s.enemy_slots = {3, 2};
      s.enemies[2].flags = flags;
      s.enemies[3].flags = 3;
      const size_t word = (progress & 0x3ff) >> 5;
      const uint32_t mask = 0x80000000u >> (progress & 31);
      s.flags[3][word] = ~mask;
      const auto before = s.flags;
      run_scenario(program(1, progress), s);
      auto expected = before;
      if (flags & 2)
        expected[3][word] |= mask;
      require(s.flags == expected && s.enemies[2].flags == flags &&
                  s.room_case == 7,
              "Original thirteen sets only ten-bit MSB-first persistent "
              "defeated flag from authored enemy work");
    }
  }
  ScenarioState persisted;
  persisted.flags[3][31] = 1;
  run_scenario(program(255, 1023), persisted);
  require(persisted.room_case == 7 && persisted.flags[3][31] == 1,
          "Previously recorded defeat bypasses unavailable enemy map");
}
void validation() {
  ScenarioState s;
  s.enemies.resize(1);
  s.enemies[0].flags = 3;
  s.enemy_slots = {0};
  s.flags[3][0] = 0x100;
  const auto before = s.flags;
  for (const auto &code : {Bytes{13}, Bytes{13, 0}, Bytes{13, 0, 0}}) {
    rejected([&] { run_scenario(code, s); });
    require(s.flags == before && s.enemies[0].flags == 3,
            "Incomplete instruction cannot change defeat or entity state");
  }
  rejected([&] { run_scenario(program(1, 0), s); });
  s.enemy_slots[0] = 9;
  rejected([&] { run_scenario(program(0, 0), s); });
  require(s.flags == before,
          "Invalid authored/work slot cannot publish defeated bit");
}
void reference() {
  for (const auto &sample : enemy_defeat_reference::cases) {
    ScenarioState s;
    s.enemies.resize(4);
    s.enemy_slots = {3, 2};
    s.enemies[2].flags = sample.enemy_flags;
    s.enemies[3].flags = 2;
    const size_t word = (sample.progress & 0x3ff) >> 5;
    s.flags[3][word] = sample.initial;
    run_scenario(program(1, sample.progress), s);
    require(s.flags[3][word] == sample.expected && sample.consumed == 4 &&
                s.room_case == 7,
            "Original unchanged bhDieCk reference differs from native "
            "persistent flags or extent");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "reconciliation")
      reconciliation();
    else if (mode == "validation")
      validation();
    else if (mode == "reference")
      reference();
    else
      throw std::runtime_error("Unknown defeat case");
    std::cout << "PASS " << mode << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
