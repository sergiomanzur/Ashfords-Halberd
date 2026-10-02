#pragma once
#include "assets.hpp"
#include <array>
#include <memory>
#include <optional>
namespace ashford {
class RoomScenario;
class ScenarioProgram {
public:
  size_t size() const { return block_->size() - offset_; }
  uint8_t operator[](size_t index) const;

private:
  ScenarioProgram() = default;
  std::shared_ptr<const Bytes> block_;
  size_t offset_ = 0;
  friend class RoomScenario;
  friend RoomScenario decode_room_scenario(const Bytes &data);
};
// EVT_WORK disk offsets are relative to the event block, not process pointers.
// Slots 0/1 contain room initialization/control; event IDs start at slot 2.
class RoomScenario {
public:
  size_t entry_count() const { return programs_.size(); }
  const ScenarioProgram &program(size_t slot) const;
  const ScenarioProgram &event(size_t id) const;

private:
  std::vector<ScenarioProgram> programs_;
  friend RoomScenario decode_room_scenario(const Bytes &data);
};
RoomScenario decode_room_scenario(const Bytes &data);
struct ScenarioDoor {
  uint16_t attribute = 0;
  uint8_t stage = 0, room = 0, position = 0, type = 0;
};
struct ScenarioState {
  std::array<std::array<uint32_t, 32>, 17> flags{};
  uint8_t room_case = 0, interaction_index = 0, floor_index = 0;
  std::optional<ScenarioDoor> door;
};
// Native synchronous control subset. Unsupported instructions fail explicitly;
// task scheduling, entity controllers and audiovisual instructions are pending.
void run_scenario(const Bytes &program, ScenarioState &state);
void run_scenario(const ScenarioProgram &program, ScenarioState &state);
} // namespace ashford
