// Native EF_WRK decoding and allocation from MIT recvx-decomp room.c/effect.c.
#include "effects.hpp"
#include "room.hpp"
#include "scenario.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ashford {
namespace {
struct Reader {
  const Bytes &data;
  void range(size_t p, size_t n) const {
    if (p > data.size() || n > data.size() - p)
      throw std::runtime_error("Room effect record outside data");
  }
  uint32_t u32(size_t p) const {
    range(p, 4);
    return uint32_t(data[p]) | uint32_t(data[p + 1]) << 8 |
           uint32_t(data[p + 2]) << 16 | uint32_t(data[p + 3]) << 24;
  }
  int32_t s32(size_t p) const {
    const auto v = u32(p);
    return v < 0x80000000u ? int32_t(v) : int32_t(int64_t(v) - 0x100000000ll);
  }
  uint16_t u16(size_t p) const {
    range(p, 2);
    return uint16_t(uint16_t(data[p]) | uint16_t(data[p + 1]) << 8);
  }
  int16_t s16(size_t p) const {
    const auto v = u16(p);
    return int16_t(v < 32768 ? int32_t(v) : int32_t(v) - 65536);
  }
  float real(size_t p) const {
    const auto bits = u32(p);
    float result;
    std::memcpy(&result, &bits, 4);
    if (!std::isfinite(result))
      throw std::runtime_error("Nonfinite room effect value");
    return result;
  }
  Vec3 vector(size_t p) const { return {real(p), real(p + 4), real(p + 8)}; }
};
} // namespace
std::vector<RoomEffectRecord> decode_room_effects(const Bytes &data) {
  room_info(data);
  Reader reader{data};
  const size_t rom = reader.u32(16);
  const size_t count = reader.u32(rom + 0x94), table = reader.u32(rom + 0x14);
  if (count > 512 || (count && !table))
    throw std::runtime_error("Invalid room effect table");
  reader.range(table, count * 68);
  std::vector<RoomEffectRecord> result;
  result.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const size_t p = table + i * 68;
    RoomEffectRecord record;
    record.flags = reader.u32(p);
    record.id = reader.u16(p + 4);
    record.type = reader.u16(p + 6);
    if (record.id >= 450)
      throw std::runtime_error("Room effect model ID outside source pool");
    record.floor = reader.s16(p + 8);
    record.model_version = reader.u16(p + 10);
    record.position = reader.vector(p + 12);
    record.scale = reader.vector(p + 24);
    record.angles = {reader.s16(p + 38), reader.s16(p + 36)};
    record.link_type = reader.s32(p + 40);
    record.link_index = reader.s32(p + 44);
    record.linked_joint = reader.s32(p + 48);
    record.offset = reader.vector(p + 52);
    record.parameter = reader.s32(p + 64);
    result.push_back(record);
  }
  return result;
}
void bind_room_effects(const Bytes &data, ScenarioState &state) {
  const auto records = decode_room_effects(data);
  for (const auto &record : records)
    if (record.link_type != 0)
      throw std::runtime_error(
          "Authored room effect links require their native model/link service");
  if (records.empty()) {
    state.effect_slots.clear();
    return;
  }
  auto pool = state.effect_pool
                  ? std::make_shared<NativeEffectPool>(*state.effect_pool)
                  : std::make_shared<NativeEffectPool>();
  std::vector<size_t> slots;
  slots.reserve(records.size());
  for (const auto &record : records) {
    size_t slot = 0;
    while (slot < pool->works.size() && (pool->works[slot].flags & 3))
      ++slot;
    if (slot == pool->works.size())
      throw std::runtime_error("Authored room effects exceed native work pool");
    NativeEffectWork work;
    work.flags = record.flags & ~0x80u;
    work.id = record.id;
    work.type = record.type;
    const auto floor = uint8_t(uint16_t(record.floor));
    work.floor = int8_t(floor < 128 ? int(floor) : int(floor) - 256);
    work.model_version = uint8_t(record.model_version);
    work.texture_id = -1;
    work.position = record.position;
    work.scale = work.base_scale = record.scale;
    work.angles = {record.angles[0], record.angles[1], 0};
    work.linked_joint = record.linked_joint;
    pool->works[slot] = std::move(work);
    slots.push_back(slot);
  }
  state.effect_pool = std::move(pool);
  state.effect_slots = std::move(slots);
}
void set_room_effect_visibility(ScenarioState &state, size_t authored,
                                bool visible) {
  if (!state.effect_pool || authored >= state.effect_slots.size() ||
      state.effect_slots[authored] >= state.effect_pool->works.size())
    throw std::runtime_error("Room effect visibility outside authored map");
  const auto slot = state.effect_slots[authored];
  const auto prior = state.effect_pool->works[slot].state_flags;
  const auto next = visible ? prior & ~0x1000000u : prior | 0x1000000u;
  if (next == prior)
    return;
  auto pool = std::make_shared<NativeEffectPool>(*state.effect_pool);
  pool->works[slot].state_flags = next;
  state.effect_pool = std::move(pool);
}
void set_room_effect_mode(ScenarioState &state, size_t authored, uint8_t mode) {
  if (!state.effect_pool || authored >= state.effect_slots.size() ||
      state.effect_slots[authored] >= state.effect_pool->works.size())
    throw std::runtime_error("Room effect mode outside authored map");
  const auto slot = state.effect_slots[authored];
  if (state.effect_pool->works[slot].modes[1] == mode)
    return;
  auto pool = std::make_shared<NativeEffectPool>(*state.effect_pool);
  pool->works[slot].modes[1] = mode;
  state.effect_pool = std::move(pool);
}
void set_room_effect_type(ScenarioState &state, size_t authored, uint8_t type) {
  if (!state.effect_pool || authored >= state.effect_slots.size() ||
      state.effect_slots[authored] >= state.effect_pool->works.size())
    throw std::runtime_error("Room effect type outside authored map");
  const auto slot = state.effect_slots[authored];
  if (state.effect_pool->works[slot].type == type)
    return;
  auto pool = std::make_shared<NativeEffectPool>(*state.effect_pool);
  pool->works[slot].type = type;
  state.effect_pool = std::move(pool);
}
} // namespace ashford
