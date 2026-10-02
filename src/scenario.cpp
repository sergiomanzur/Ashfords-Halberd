#include "scenario.hpp"
#include "room.hpp"
#include <limits>
#include <stdexcept>
namespace ashford {
void bind_room_enemies(const Bytes &data, const RoomCollision &collision,
                       ScenarioState &state) {
  auto records = decode_room_records(data);
  auto models = decode_room_enemy_models(data);
  auto motions = decode_room_enemy_motion_banks(data);
  std::vector<ScenarioEntity> enemies(128);
  std::vector<size_t> slots;
  size_t active = 0;
  for (size_t i = 0; i < records.enemies.size(); ++i) {
    const auto &authored = records.enemies[i];
    if (authored.work_number >= enemies.size())
      throw std::runtime_error("Authored enemy work number outside runtime");
    slots.push_back(authored.work_number);
    if (!(authored.flags & 1))
      continue;
    auto &entity = enemies.at(active++);
    entity.flags = authored.flags;
    entity.id = authored.id;
    entity.type = authored.type;
    entity.model_version = authored.model_version;
    if (entity.id > 40) {
      entity.model_flags |= 0x20;
      entity.flags |= 0x8000;
    }
    if (entity.id > 90)
      entity.model_flags |= 0x100;
    entity.position = authored.position;
    entity.angles = {0, authored.rotation_y, 0};
    entity.floor = player_floor_number(collision, entity.position.y);
    entity.model_bank =
        std::make_shared<const ActorModelBank>(std::move(models.at(i)));
    if (!entity.model_bank->models.empty())
      entity.model = std::shared_ptr<const ActorModel>(
          entity.model_bank, &entity.model_bank->models[0]);
  }
  // Original finish-room loading visits authored-count runtime slots,
  // including zeroed trailing slots, independently of model allocation.
  for (size_t i = 0; i < motions.size(); ++i)
    enemies.at(i).motions = motions[i];
  state.enemies = std::move(enemies);
  state.enemy_slots = std::move(slots);
}
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
                       ScenarioTask *task = nullptr, bool scheduled = true) {
  size_t pc = task && scheduled ? task->pc : 0;
  // The original G_Sp is a branch-target cursor, not a stack of targets.
  // Nested conditionals replace it; ENDIF/ELSE subtract four bytes.
  auto &branch_cursor = state.branch_cursor;
  auto &depth = state.conditional_depth;
  size_t origin = program_origin(program);
  auto finish = [&] {
    if (task && scheduled)
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
    case 56: {
      extent(6);
      int32_t frame;
      switch (program[pc + 1]) {
      case 0:
        frame = state.player_frame;
        break;
      case 1:
        frame =
            indexed(state.enemies, indexed(state.enemy_slots, program[pc + 2]))
                .frame;
        break;
      case 2:
        frame = indexed(state.objects, program[pc + 2]).frame;
        break;
      default:
        throw std::runtime_error("Unsupported scenario frame-check group");
      }
      uint32_t bits = uint32_t(word(pc + 4)) << 16;
      if (program[pc + 3])
        bits += 32768;
      int32_t target = bits < 0x80000000u
                           ? int32_t(bits)
                           : int32_t(int64_t(bits) - 0x100000000ll);
      pc += 6;
      return frame < target;
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
    case 56:
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
    case 101: {
      extent(4);
      if (!task)
        throw std::runtime_error("Scenario work binding requires event task");
      uint8_t group = program[pc + 1];
      size_t index = program[pc + 2];
      ScenarioEntity *entity = nullptr;
      switch (group) {
      case 1:
        index = indexed(state.enemy_slots, index);
        entity = &indexed(state.enemies, index);
        break;
      case 2:
        entity = &indexed(state.objects, index);
        break;
      case 3:
        entity = &indexed(state.items, index);
        break;
      default:
        throw std::runtime_error("Unsupported scenario work binding group");
      }
      task->work_group = group;
      task->work_index = index;
      task->model_index = group == 3 ? 0 : program[pc + 3];
      if (group == 1) {
        task->motion_mode = entity->motion_mode;
        task->model_flags = entity->model_flags;
        task->blend_rate = entity->blend_rate;
      }
      pc += 4;
      break;
    }
    case 105: {
      extent(2);
      if (!task)
        throw std::runtime_error(
            "Scenario common controller requires event task");
      auto operation = program[pc + 1];
      if (operation == 12 || operation == 13) {
        extent(6);
        auto &signs =
            operation == 12 ? task->position_signs : task->angle_signs;
        for (size_t i = 0; i < 3; ++i)
          signs[i] = program[pc + 2 + i];
        pc += 6;
        break;
      }
      if (operation != 7 && operation != 11 && operation != 47 &&
          operation != 24)
        throw std::runtime_error("Unsupported scenario common controller " +
                                 std::to_string(operation));
      extent(operation == 7 || operation == 24 ? 8 : operation == 11 ? 6 : 2);
      if (!task->work_index)
        throw std::runtime_error(
            "Scenario common controller without bound work");
      auto &entity =
          task->work_group == 1   ? indexed(state.enemies, *task->work_index)
          : task->work_group == 2 ? indexed(state.objects, *task->work_index)
          : task->work_group == 3
              ? indexed(state.items, *task->work_index)
              : throw std::runtime_error(
                    "Unsupported bound scenario work group");
      if (operation == 24) {
        if (program[pc + 3] != 1 || program[pc + 6] != 1 || entity.id == 1)
          throw std::runtime_error(
              "Unsupported scenario motion selection mode or companion");
        if (!state.room_motions || state.room_motions->size() != 512)
          throw std::runtime_error(
              "Scenario motion selection without room bank");
        static constexpr uint32_t steps[] = {
            0x8000, 0x10000, 0x10000, 0x8000, 0x5555, 0x4000, 0x3333,
            0x2aaa, 0x8000,  0x4000,  0x2aaa, 0x2000, 0x1999, 0x1555,
            0x2492, 0x2000,  0x1c71,  0x1999, 0x1745, 0x1555, 0x1249,
            0x1000, 0xe38,   0xccc,   0xba2,  0xaaa};
        auto speed = program[pc + 2];
        if (speed >= std::size(steps))
          throw std::runtime_error("Unsupported scenario motion speed");
        auto motion = program[pc + 4], blend = program[pc + 7];
        entity.motions = state.room_motions;
        entity.motion_step = steps[speed];
        entity.modes = {uint8_t(program[pc + 5] ? 5 : 1), motion, 0, 0};
        entity.motion_number = (entity.motion_number & ~0xffu) | motion;
        entity.motion_mode = speed == 1 || speed == 2 ? 0x20 : 0x28;
        entity.blend_rate = 0;
        entity.control1 = 1;
        entity.control2 = blend > 8 ? 10 : blend;
        if (blend == 6)
          entity.motion_mode |= 8;
        else if (blend > 8)
          entity.blend_count = blend;
        entity.frame = 0;
        pc += 8;
      } else if (operation == 47) {
        if (!entity.model)
          throw std::runtime_error(
              "Scenario model rebuild without bound primary model");
        if (entity.flags & 0x1080)
          throw std::runtime_error(
              "Unsupported linked or retained scenario actor matrix");
        entity.rendered_model = pose_actor_rotated(
            *entity.model, entity.joint_pose, entity.position, entity.angles);
        pc += 2;
      } else if (operation == 7) {
        if (!state.collision)
          throw std::runtime_error(
              "Scenario position requires room floor data");
        auto component = [&](size_t i) {
          float value = word(pc + 2 + i * 2) / 100.0f;
          return task->position_signs[i] ? value * -1.0f : value;
        };
        entity.position = {component(0), component(1), component(2)};
        entity.previous_position = entity.position;
        entity.floor = player_floor_number(*state.collision, entity.position.y);
        pc += 8;
      } else {
        for (size_t i = 0; i < 3; ++i) {
          int value = program[pc + 2 + i];
          if (task->angle_signs[i])
            value = -value;
          entity.angles[i] = int32_t(value * 182.04445f);
        }
        entity.previous_angles = entity.angles;
        pc += 6;
      }
      break;
    }
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
  for (size_t level = 0; level < state.tasks.size(); ++level) {
    state.current_task = level;
    auto &task = state.tasks[level];
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
}
void initialize_scenario(ScenarioState &state) {
  if (!state.scenario)
    throw std::runtime_error("No bound room scenario");
  const auto &initialization = state.scenario->program(0);
  for (auto &task : state.tasks)
    task.active = false;
  tick_scenario_actors(state);
  state.current_task = 0;
  state.conditional_depth = 0;
  state.event_timer = 0;
  // bhScenarioCheck uses task 0's command context but does not store the
  // running script pointer into its data field, unlike the scheduler.
  run_scenario_impl(initialization, state, &state.tasks[0], false);
  state.flags[6][0] |= 0x10;
  try {
    tick_scenario_tasks(state);
  } catch (...) {
    state.flags[6][0] &= ~0x10u;
    throw;
  }
  state.flags[6][0] &= ~0x10u;
}
void control_scenario(ScenarioState &state) {
  if (!state.scenario || state.current_task >= state.tasks.size())
    throw std::runtime_error("Invalid room control context");
  const auto &control = state.scenario->program(1);
  state.conditional_depth = 0;
  run_scenario_impl(control, state, &state.tasks[state.current_task], false);
  tick_scenario_tasks(state);
}
void tick_scenario_actors(ScenarioState &state) {
  if (!(state.flags[6][0] & 2))
    return;
  for (auto &entity : state.enemies) {
    if (!(entity.flags & 1) || (entity.state_flags & 0x41000000))
      continue;
    if (entity.id != 67 || entity.modes[0] > 1 ||
        (entity.modes[0] == 1 &&
         (entity.modes[2] != 0 ||
          (entity.control2 != 0 && entity.control2 != 10))))
      throw std::runtime_error("Unsupported scenario actor controller");
    if (!entity.model || !entity.motions || (entity.flags & 0x1080) ||
        (entity.motion_mode & 2) || entity.blend_count < 0)
      throw std::runtime_error("Unsupported scenario actor motion binding");
    entity.previous_position = entity.position;
    entity.previous_angles = entity.angles;
    entity.flags = (entity.flags | 0x8100) & ~2u;
    bool update_motion = true;
    if (!entity.modes[0]) {
      // subpl.c::init_subpl -> em60_init samples once, then holds mode3=3.
      entity.modes[0] = 1;
      entity.modes[3] = 3;
      entity.blend_rate = 0;
      entity.blend_count = 0;
      entity.motion_step = 32768;
      entity.frame = 0;
      entity.motion_number = 0;
      entity.motion_mode = 0x20;
      entity.frame_direction = 0;
    } else if (!entity.modes[3]) {
      entity.modes[3] = 1;
      entity.motion_number = entity.modes[1];
      if (entity.control2 == 0)
        entity.blend_count = 8;
      entity.blend_rate = 0;
      entity.motion_mode &= ~0x100u;
      entity.frame_direction = 0;
    } else {
      update_motion = entity.modes[3] == 1;
    }
    if (!update_motion) {
      entity.rendered_model = pose_actor_rotated(
          *entity.model, entity.joint_pose, entity.position, entity.angles);
      continue;
    }
    if (entity.motion_number >= entity.motions->size())
      throw std::runtime_error("Scenario actor motion outside bank");
    const auto &slot = (*entity.motions)[entity.motion_number];
    entity.motion_boundary = 0;
    if (slot) {
      const auto &motion = *slot;
      if (entity.frame < 0 ||
          uint64_t(entity.frame) >= uint64_t(motion.frames) * 65536)
        throw std::runtime_error("Scenario actor frame outside motion");
      if (entity.joint_pose.empty())
        entity.joint_pose = actor_bind_pose(*entity.model);
      bool slow = (entity.motion_mode & 8) || (motion.flags & 0x20);
      auto target = sample_motion(motion, entity.frame, slow);
      if (target.size() > 64 || (!slow && !(motion.flags & 0x10) &&
                                 (target.empty() || !target[0].position)))
        throw std::runtime_error("Unsupported scenario actor channel mapping");
      if (target.size() > entity.joint_pose.size())
        throw std::runtime_error("Scenario actor tracks outside skeleton");
      for (size_t i = 0; i < target.size(); ++i) {
        if (!slow && !(motion.flags & 0x10) && i)
          target[i].position.reset();
        if (!target[i].angles)
          target[i].angles = std::array<uint16_t, 3>{};
      }
      if (entity.blend_count) {
        uint32_t remaining_bits = 65536u - uint32_t(entity.blend_rate);
        int32_t remaining_rate =
            remaining_bits < 0x80000000u
                ? int32_t(remaining_bits)
                : int32_t(int64_t(remaining_bits) - 0x100000000ll);
        float rate =
            entity.motion_mode & 0x20
                ? 1.f / entity.blend_count - 0.000015258789f * entity.blend_rate
                : 0.000015258789f * remaining_rate;
        entity.joint_pose = blend_motion_pose(entity.joint_pose, target, rate,
                                              entity.motion_mode & 0x100);
        --entity.blend_count;
      } else {
        for (size_t i = 0; i < target.size(); ++i) {
          if (target[i].position)
            entity.joint_pose[i].position = target[i].position;
          if (target[i].angles)
            entity.joint_pose[i].angles = target[i].angles;
        }
      }
      if (!motion.attributes.empty())
        entity.motion_attribute =
            motion.attributes.at(size_t(entity.frame) >> 16);
      entity.motion_boundary = advance_motion_frame(
          motion.frames, int32_t(entity.motion_step), entity.motion_mode,
          entity.frame, entity.frame_direction);
    }
    entity.rendered_model = pose_actor_rotated(*entity.model, entity.joint_pose,
                                               entity.position, entity.angles);
  }
}
} // namespace ashford
