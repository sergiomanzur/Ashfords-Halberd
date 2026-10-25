#pragma once
#include "runtime.hpp"
#include <filesystem>
#include <vector>

namespace ashford {

namespace fs = std::filesystem;

inline constexpr uint32_t SAVE_MAGIC = 0x46485341u; // 'ASHF'
// Version 1 used an ambiguous packed-ammunition header and omitted the active
// player bank/difficulty. Reject it instead of silently changing item ownership.
inline constexpr uint32_t SAVE_VERSION = 2;

struct SaveHeader {
  uint32_t magic = SAVE_MAGIC;
  uint32_t version = SAVE_VERSION;
  uint32_t payload_size = 0;
  uint32_t checksum = 0;
};

// Serializes navigation, player status, carried/box inventory and flag banks.
// Actor/tasks/effect/audio continuation is a separate runtime-save milestone.
Bytes serialize_save_state(const RoomRuntime &runtime);

// Deserializes and validates binary buffer into scenario state and player world.
// Throws std::runtime_error on corrupted data, version mismatch, or invalid checksum.
void deserialize_save_state(const Bytes &data, ScenarioState &out_scenario,
                            PlayerWorldState &out_world);

// Saves to a file atomically (writing to .tmp first, then renaming).
void save_game_to_file(const fs::path &path, const RoomRuntime &runtime);

// Loads from a file. Throws on error or file not found.
void load_game_from_file(const fs::path &path, ScenarioState &out_scenario,
                         PlayerWorldState &out_world);

} // namespace ashford
