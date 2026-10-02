#include "scenario.hpp"
#include <limits>
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
size_t program_origin(const Bytes &) { return 0; }
size_t program_origin(const ScenarioProgram &program) {
  return program.origin();
}
template <class Program>
void run_scenario_impl(const Program &program, ScenarioState &state,
                       ScenarioTask *task = nullptr) {
  size_t pc = task ? task->pc : 0;
  // The original G_Sp is a branch-target cursor, not a stack of targets.
  // Nested conditionals replace it; ENDIF/ELSE subtract four bytes.
  auto &branch_cursor = state.branch_cursor;
  auto &depth = state.conditional_depth;
  size_t origin = program_origin(program);
  auto finish = [&] {
    if (task)
      task->pc = pc;
  };
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
  auto resume = [&] {
    if (depth <= 0) {
      finish();
      return false;
    }
    branch_cursor -= 4;
    --depth;
    if (branch_cursor < int64_t(origin))
      throw std::runtime_error("Scenario branch before program");
    jump(size_t(branch_cursor - int64_t(origin)));
    return true;
  };
  auto condition = [&]() -> bool {
    extent(1);
    switch (program[pc]) {
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
      return pass;
    }
    case 7: {
      extent(6);
      int32_t value = 0;
      switch (program[pc + 2]) {
      case 5:
        value = state.player_health;
        break;
      case 7:
        value = state.message_selection;
        break;
      case 9:
        value = state.status_timer;
        break;
      case 10:
        value =
            indexed(state.enemies, indexed(state.enemy_slots, program[pc + 1]))
                .health;
        break;
      default:
        throw std::runtime_error("Unsupported scenario word comparison field");
      }
      auto operation = program[pc + 3];
      int32_t right = word(pc + 4);
      pc += 6;
      switch (operation) {
      case 0:
        return value == right;
      case 1:
        return value > right;
      case 2:
        return value >= right;
      case 3:
        return value < right;
      case 4:
        return value <= right;
      case 5:
        return value != right;
      default:
        throw std::runtime_error("Invalid scenario word comparison");
      }
    }
    case 17: {
      extent(2);
      auto item = program[pc + 1];
      pc += 2;
      if (state.player_id >= state.inventory.size() / 16)
        throw std::runtime_error("Scenario player inventory outside storage");
      size_t base = size_t(state.player_id) * 16;
      for (size_t i = base + 1; i < base + 12; ++i) {
        uint32_t id = state.inventory[i] >> 16;
        if (uint8_t(id) == item) {
          if (uint8_t(id) == 72 && id != 72)
            state.flags[4][0] |= 0x10000;
          return true;
        }
      }
      return false;
    }
    case 38: {
      extent(2);
      bool pass = state.equipped_weapon == program[pc + 1];
      pc += 2;
      return pass;
    }
    default:
      throw std::runtime_error("Unsupported scenario loop condition");
    }
  };
  auto decrement = [](int32_t &count) {
    count = count == std::numeric_limits<int32_t>::min()
                ? std::numeric_limits<int32_t>::max()
                : count - 1;
    return count;
  };
  auto loop_index = [&](size_t limit) -> size_t {
    if (!task || task->loop < 0 || size_t(task->loop) >= limit)
      throw std::runtime_error("Scenario invalid loop index");
    return size_t(task->loop);
  };
  auto enter_loop = [&](size_t limit) -> size_t {
    if (!task || task->loop < -1 || task->loop >= int(limit) - 1)
      throw std::runtime_error("Scenario loop nesting exceeded");
    return size_t(++task->loop);
  };
  auto jump_absolute = [&](size_t target) {
    if (target < origin)
      throw std::runtime_error("Scenario loop before program");
    jump(target - origin);
  };
  for (size_t steps = 0; steps < 100000; ++steps) {
    extent(1);
    switch (program[pc]) {
    case 0:
      depth = 0;
      finish();
      return;
    case 1: {
      extent(2);
      size_t target = pc + 2 + program[pc + 1];
      if (target >= program.size())
        throw std::runtime_error("Scenario conditional outside program");
      branch_cursor = int64_t(origin) + int64_t(target) + 4;
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
    case 4:
    case 7:
    case 17:
    case 38: {
      bool pass = condition();
      if (!pass) {
        if (!resume())
          return;
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
    case 20:
      extent(4);
      start_scenario_event(state, program[pc + 2], program[pc + 3]);
      pc += 4;
      break;
    case 182:
      extent(2);
      state.room_sound_case = program[pc + 1];
      pc += 2;
      break;
    case 188:
      extent(2);
      indexed(state.tasks, program[pc + 1]).active = false;
      pc += 2;
      break;
    case 248:
    case 249: {
      if (!task)
        throw std::runtime_error("Scenario sleep requires event task");
      if (program[pc] == 248) {
        extent(4);
        auto index = enter_loop(4);
        task->counts[index] = word(pc + 2);
        ++pc;
      } else
        extent(3);
      if (decrement(task->counts[loop_index(4)]) == 0) {
        pc += 3;
        --task->loop;
      }
      if (!resume())
        return;
      break;
    }
    case 250: {
      extent(4);
      auto index = enter_loop(4);
      uint16_t raw = word(pc + 2);
      task->count3 = task->count2 = task->counts[index] =
          raw < 32768 ? int32_t(raw) : int32_t(raw) - 65536;
      pc += 4;
      task->loop_bodies[index] = origin + pc;
      break;
    }
    case 251: {
      extent(2);
      auto index = loop_index(4);
      if (decrement(task->counts[index]) != 0) {
        task->count2 = task->counts[index];
        jump_absolute(task->loop_bodies[index]);
      } else {
        pc += 2;
        --task->loop;
      }
      break;
    }
    case 252: {
      extent(2);
      auto index = enter_loop(6);
      task->loop_conditions[index] = origin + pc + 2;
      jump(pc + program[pc + 1]);
      task->loop_bodies[index] = origin + pc;
      break;
    }
    case 253: {
      auto index = loop_index(6);
      auto end = pc;
      jump_absolute(task->loop_conditions[index]);
      if (condition())
        jump_absolute(task->loop_bodies[index]);
      else {
        pc = end + 1;
        --task->loop;
        if (!resume())
          return;
      }
      break;
    }
    case 254:
      if (!task)
        throw std::runtime_error("Scenario yield requires event task");
      ++pc;
      if (!resume())
        return;
      break;
    case 255:
      extent(2);
      if (!task)
        throw std::runtime_error("Scenario end requires event task");
      task->active = false;
      pc += 2;
      if (!resume())
        return;
      break;
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
  state.conditional_depth = 0;
  state.branch_cursor = 0;
  run_scenario_impl(program, state);
}
void run_scenario(const ScenarioProgram &program, ScenarioState &state) {
  state.conditional_depth = 0;
  state.branch_cursor = 0;
  run_scenario_impl(program, state);
}
size_t start_scenario_event(ScenarioState &state, size_t level, size_t event) {
  if (!state.scenario)
    throw std::runtime_error("No bound room scenario");
  const auto &program = state.scenario->event(event);
  if (level >= 16) {
    level = 0;
    while (level != 15 && state.tasks[level].active)
      ++level;
  }
  auto &task = state.tasks[level];
  // Event_init leaves counters and loop pointer arrays unchanged.
  task.pc = 0;
  task.loop = -1;
  task.program = program;
  task.active = true;
  return level;
}
void tick_scenario_tasks(ScenarioState &state) {
  if (!(state.flags[6][0] & 0x10))
    return;
  bool timed = false;
  for (auto &task : state.tasks)
    if (task.active) {
      if (!task.program)
        throw std::runtime_error("Active scenario task without program");
      if (!timed) {
        ++state.event_timer;
        timed = true;
      }
      // Event_init can replace this slot while its old instruction stream runs.
      // The original scheduler then stores the running pointer back into it.
      auto program = *task.program;
      run_scenario_impl(program, state, &task);
      task.program = std::move(program);
    }
}
} // namespace ashford
