#include "scenario.hpp"
#include <stdexcept>
namespace ashford {
namespace {
uint32_t u32(const Bytes &b, size_t p) {
  if (p > b.size() || b.size() - p < 4)
    throw std::runtime_error("Scenario offset outside room");
  return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
         uint32_t(b[p + 3]) << 24;
}
} // namespace
uint8_t ScenarioProgram::operator[](size_t index) const {
  if (index >= size())
    throw std::runtime_error("Scenario program index outside block");
  return (*block_)[offset_ + index];
}
const ScenarioProgram &RoomScenario::program(size_t slot) const {
  if (slot >= programs_.size())
    throw std::runtime_error("Missing scenario program");
  return programs_[slot];
}
const ScenarioProgram &RoomScenario::event(size_t id) const {
  if (programs_.size() < 2 || id >= programs_.size() - 2)
    throw std::runtime_error("Missing scenario event");
  return program(id + 2);
}
RoomScenario decode_room_scenario(const Bytes &data) {
  room_info(data);
  size_t base = u32(data, 28);
  RoomScenario result;
  if (!base)
    return result;
  size_t end = data.size();
  // The nearest following outer-header section bounds the event block.
  for (size_t header : {size_t(16), size_t(20), size_t(24), size_t(32)}) {
    size_t pointer = u32(data, header);
    if (pointer > base && pointer < end)
      end = pointer;
  }
  if (base > end || end - base < 8)
    throw std::runtime_error("Truncated scenario block");
  size_t table_size = u32(data, base);
  if (table_size < 8 || table_size % 4 || table_size > end - base ||
      table_size / 4 > 4096)
    throw std::runtime_error("Invalid scenario entry table");
  std::vector<size_t> offsets;
  for (size_t i = 0; i < table_size; i += 4) {
    size_t offset = u32(data, base + i);
    if (offset < table_size || offset >= end - base)
      throw std::runtime_error("Scenario entry outside block");
    offsets.push_back(offset);
  }
  auto block =
      std::make_shared<const Bytes>(data.begin() + base, data.begin() + end);
  for (size_t offset : offsets) {
    ScenarioProgram program;
    program.block_ = block;
    program.offset_ = offset;
    result.programs_.push_back(std::move(program));
  }
  return result;
}
template <class Program>
void run_scenario_impl(const Program &program, ScenarioState &state) {
  size_t pc = 0;
  // The original G_Sp is a branch-target cursor, not a stack of targets.
  // Nested conditionals replace it; ENDIF/ELSE subtract four bytes.
  int64_t branch_cursor = 0;
  int depth = 0;
  auto extent = [&](size_t n) {
    if (pc > program.size() || n > program.size() - pc)
      throw std::runtime_error("Truncated scenario instruction");
  };
  auto jump = [&](size_t target) {
    if (target >= program.size())
      throw std::runtime_error("Scenario branch outside program");
    pc = target;
  };
  auto word = [&](size_t p) {
    return uint16_t(uint16_t(program[p]) | uint16_t(program[p + 1]) << 8);
  };
  auto flag = [&](uint8_t bank, uint16_t count) -> uint32_t & {
    static constexpr size_t sizes[17] = {0,  32, 16, 32, 1, 1, 1, 16, 8,
                                         16, 1,  1,  1,  1, 1, 1, 1};
    size_t index = (count & 0x3ff) >> 5;
    if (bank >= 17 || index >= sizes[bank])
      throw std::runtime_error("Scenario flag outside bank");
    return state.flags[bank][index];
  };
  auto indexed = [](auto &values, size_t index) -> auto & {
    if (index >= values.size())
      throw std::runtime_error("Scenario entity index outside room");
    return values[index];
  };
  for (size_t steps = 0; steps < 100000; ++steps) {
    extent(1);
    switch (program[pc]) {
    case 0:
      return;
    case 1: {
      extent(2);
      size_t target = pc + 2 + program[pc + 1];
      if (target >= program.size())
        throw std::runtime_error("Scenario conditional outside program");
      branch_cursor = int64_t(target) + 4;
      ++depth;
      pc += 2;
      break;
    }
    case 2: {
      extent(2);
      branch_cursor -= 4;
      --depth;
      jump(pc + program[pc + 1]);
      break;
    }
    case 3:
      extent(2);
      branch_cursor -= 4;
      --depth;
      pc += 2;
      break;
    case 4: {
      extent(6);
      auto bank = program[pc + 1];
      auto count = word(pc + 2);
      bool value = (flag(bank, count) & (0x80000000u >> (count & 31))) != 0;
      bool pass = bool(program[pc + 5] ^ uint8_t(value));
      if (bank == 10 &&
          ((program[pc + 2] == 23 &&
            state.interaction_index != program[pc + 4]) ||
           (program[pc + 2] == 22 && state.floor_index != program[pc + 4])))
        pass = false;
      pc += 6;
      if (!pass) {
        if (depth <= 0)
          return;
        branch_cursor -= 4;
        --depth;
        if (branch_cursor < 0)
          throw std::runtime_error("Scenario branch before program");
        jump(size_t(branch_cursor));
      }
      break;
    }
    case 5: {
      extent(6);
      auto count = word(pc + 2);
      auto &value = flag(program[pc + 1], count);
      auto operation = program[pc + 5];
      if (operation > 5)
        throw std::runtime_error("Invalid scenario flag operation");
      uint32_t mask =
          (operation < 3 ? 0x80000000u : 0xffffffffu) >> (count & 31);
      switch (operation % 3) {
      case 0:
        value |= mask;
        break;
      case 1:
        value &= ~mask;
        break;
      case 2:
        value ^= mask;
        break;
      }
      pc += 6;
      break;
    }
    case 14: {
      extent(6);
      auto count = word(pc + 2);
      auto interaction = program[pc + 4];
      auto keep_trigger = program[pc + 5];
      pc += 6;
      if (state.interaction_index != interaction)
        break;
      auto &item_flags = flag(7, count);
      uint32_t mask = 0x80000000u >> (count & 31);
      if (state.flags[10][0] & 0x800) {
        if (!(item_flags & mask)) {
          auto &trigger = indexed(state.interactions, interaction);
          auto &item = indexed(state.items, trigger.parameters[0]);
          if (!keep_trigger)
            trigger.flags &= uint8_t(~1u);
          item.state_flags |= 0x1000000;
          item_flags |= mask;
          flag(9, count & 0x1ff) &= ~mask;
        }
      } else if (!(item_flags & mask)) {
        flag(9, count) |= mask;
      }
      state.flags[10][0] &= ~0x800u;
      break;
    }
    case 35: {
      extent(6);
      auto item_index = program[pc + 1];
      auto count = word(pc + 2);
      auto &trigger = indexed(state.interactions, program[pc + 4]);
      if (flag(7, count & 0x1ff) & (0x80000000u >> (count & 31))) {
        auto &item = indexed(state.items, item_index);
        if (program[pc + 5] == 0)
          trigger.flags &= uint8_t(~1u);
        item.state_flags |= 0x1000000;
      } else
        trigger.flags |= 1;
      pc += 6;
      break;
    }
    case 36: {
      extent(4);
      auto group = program[pc + 1], index = program[pc + 2],
           enabled = program[pc + 3];
      uint32_t *status = nullptr;
      ScenarioEntity *enemy = nullptr;
      switch (group) {
      case 0:
        status = &state.flags[14][0];
        break;
      case 1:
        enemy = &indexed(state.enemies, indexed(state.enemy_slots, index));
        status = &enemy->state_flags;
        break;
      case 2:
        status = &indexed(state.objects, index).state_flags;
        break;
      case 3:
        status = &indexed(state.items, index).state_flags;
        break;
      default:
        throw std::runtime_error("Unsupported scenario entity group");
      }
      if (!enabled) {
        *status |= 0x1000000;
        if (group == 0 && state.stage == 9 && state.room == 30 &&
            state.room_case == 2)
          state.player_sleep_ticks = 2;
        if (enemy)
          enemy->flags |= 0x8000;
      } else {
        *status &= ~0x1000000u;
        if (enemy && !(enemy->flags & 0x80000000))
          enemy->flags &= ~0x8000u;
      }
      pc += 4;
      break;
    }
    case 37: {
      extent(10);
      auto &trigger = indexed(state.interactions, program[pc + 1]);
      trigger.attribute = word(pc + 2);
      for (size_t i = 0; i < 4; ++i)
        trigger.parameters[i] = program[pc + 4 + i];
      trigger.type = program[pc + 8];
      pc += 10;
      break;
    }
    case 66:
      extent(4);
      indexed(state.items, program[pc + 1]).angular_speed = program[pc + 2];
      pc += 4;
      break;
    case 51:
      extent(8);
      state.door = ScenarioDoor{word(pc + 2), program[pc + 4], program[pc + 5],
                                program[pc + 6], program[pc + 7]};
      pc += 8;
      break;
    case 55:
      extent(2);
      state.room_case = program[pc + 1];
      pc += 2;
      break;
    default:
      throw std::runtime_error("Unsupported scenario opcode " +
                               std::to_string(program[pc]) + " at " +
                               std::to_string(pc));
    }
  }
  throw std::runtime_error("Scenario instruction budget exhausted");
}
void run_scenario(const Bytes &program, ScenarioState &state) {
  run_scenario_impl(program, state);
}
void run_scenario(const ScenarioProgram &program, ScenarioState &state) {
  run_scenario_impl(program, state);
}
} // namespace ashford
