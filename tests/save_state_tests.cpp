#include "save_state.hpp"
#include <iostream>
#include <stdexcept>

using namespace ashford;

namespace {

void require(bool value, const char *msg) {
  if (!value)
    throw std::runtime_error(msg);
}

void test_round_trip() {
  RoomRuntime r;
  r.scenario.stage = 0;
  r.scenario.room = 1;
  r.scenario.floor_index = 0;
  r.world.floor = 0;
  r.world.player.position = {12.5f, 3.0f, -45.0f};
  r.world.player.yaw = 16384;
  r.world.player.motion = 2;
  r.world.player.frame = 131072;
  r.scenario.player_health = 160;
  r.scenario.spray_use_count = 7;
  r.scenario.equipped_weapon = 4;
  r.scenario.player_work_flags = 0x118;
  r.scenario.player_state_flags = 0x200;
  r.scenario.player_secondary_flags = 0x400;
  r.scenario.player_id = 1;
  r.scenario.game_mode = 2;
  r.scenario.room_case = 3;
  r.scenario.room_sound_case = 4;

  // Original bank header selects the equipped carried word, never an ammo mirror.
  r.scenario.inventory[16] = 2;
  r.scenario.inventory[17] = (55u << 16) | 1u;
  r.scenario.inventory[18] = (9u << 16) | 11u;
  r.scenario.inventory[19] = (12u << 16) | 30u;

  // Set up persistent defeat flag in bank 3
  r.scenario.flags[3][0] = 0x80000000u;
  // Set up item pickup flag in bank 7
  r.scenario.flags[7][3] = 0x18u;

  auto bytes = serialize_save_state(r);
  require(!bytes.empty(), "Serialized buffer not empty");

  ScenarioState loaded_scenario;
  PlayerWorldState loaded_world;
  deserialize_save_state(bytes, loaded_scenario, loaded_world);

  require(loaded_scenario.stage == 0, "Stage matched");
  require(loaded_scenario.spray_use_count == 7, "Recovery usage counter matched");
  require(loaded_scenario.room == 1, "Room matched");
  require(loaded_world.player.position.x == 12.5f, "Player pos x matched");
  require(loaded_world.player.position.y == 3.0f, "Player pos y matched");
  require(loaded_world.player.position.z == -45.0f, "Player pos z matched");
  require(loaded_world.player.yaw == 16384, "Player yaw matched");
  require(loaded_world.player.motion == 2, "Player motion matched");
  require(loaded_world.player.frame == 131072, "Player frame matched");
  require(loaded_scenario.player_health == 160, "Player health matched");
  require(loaded_scenario.equipped_weapon == 4, "Equipped weapon matched");
  require(loaded_scenario.player_id == 1, "Active player bank matched");
  require(loaded_scenario.game_mode == 2, "Recovery/pickup difficulty matched");
  require(loaded_scenario.room_case == 3, "Room variant matched");
  require(loaded_scenario.room_sound_case == 4, "Sound variant matched");
  require(loaded_scenario.inventory[16] == 2, "Equipped selector matched");
  require(loaded_scenario.inventory[17] == ((55u << 16) | 1u), "Extra slot matched");
  require(loaded_scenario.inventory[18] == ((9u << 16) | 11u), "Carried ammunition matched");
  require(loaded_scenario.inventory[19] == ((12u << 16) | 30u), "Reserve ammunition matched");
  require(loaded_scenario.flags[3][0] == 0x80000000u, "Defeat persistence matched");
  require(loaded_scenario.flags[7][3] == 0x18u, "Pickup persistence matched");
}

void test_corrupted_data() {
  RoomRuntime r;
  auto bytes = serialize_save_state(r);

  // Corrupt a byte in the payload
  bytes[20] ^= 0xFF;

  ScenarioState loaded_scenario;
  PlayerWorldState loaded_world;
  bool failed = false;
  try {
    deserialize_save_state(bytes, loaded_scenario, loaded_world);
  } catch (const std::runtime_error &e) {
    failed = true;
  }
  require(failed, "Corrupted checksum was caught");
}

void test_truncated_data() {
  RoomRuntime r;
  auto bytes = serialize_save_state(r);
  bytes.resize(bytes.size() - 10);

  ScenarioState loaded_scenario;
  PlayerWorldState loaded_world;
  bool failed = false;
  try {
    deserialize_save_state(bytes, loaded_scenario, loaded_world);
  } catch (const std::runtime_error &) {
    failed = true;
  }
  require(failed, "Truncated save rejected");
}

void test_magic_mismatch() {
  RoomRuntime r;
  auto bytes = serialize_save_state(r);
  bytes[0] = 'X';

  ScenarioState loaded_scenario;
  PlayerWorldState loaded_world;
  bool failed = false;
  try {
    deserialize_save_state(bytes, loaded_scenario, loaded_world);
  } catch (const std::runtime_error &) {
    failed = true;
  }
  require(failed, "Bad magic rejected");
}

void test_version_mismatch() {
  RoomRuntime r;
  auto bytes = serialize_save_state(r);
  bytes[4] = 1; // Reject the earlier packed-header prototype representation.

  ScenarioState loaded_scenario;
  PlayerWorldState loaded_world;
  bool failed = false;
  try {
    deserialize_save_state(bytes, loaded_scenario, loaded_world);
  } catch (const std::runtime_error &) {
    failed = true;
  }
  require(failed, "Bad version rejected");
}

void test_invalid_inventory_owner() {
  RoomRuntime runtime;
  runtime.scenario.inventory[0] = 2;
  runtime.scenario.inventory[2] = (9u << 16) | 15u;
  runtime.scenario.equipped_weapon = 2; // Knife index cannot own this gun.
  const auto bytes = serialize_save_state(runtime);
  ScenarioState target;
  target.player_health = 90;
  PlayerWorldState world;
  bool rejected = false;
  try { deserialize_save_state(bytes, target, world); }
  catch (const std::runtime_error &) { rejected = true; }
  require(rejected && target.player_health == 90,
          "Checksummed but inconsistent weapon ownership rejected transactionally");
}

void test_file_io() {
  auto temp_dir = fs::path("local/inventory-save-tests");
  fs::create_directories(temp_dir);
  auto save_path = temp_dir / "slot0.dat";

  RoomRuntime r;
  r.scenario.stage = 0;
  r.scenario.room = 2; // Graveyard
  r.scenario.player_health = 180;
  r.scenario.flags[3][1] = 0x40000000u; // Defeated enemy 2

  save_game_to_file(save_path, r);
  require(fs::exists(save_path), "Save file created");

  ScenarioState loaded_scenario;
  PlayerWorldState loaded_world;
  load_game_from_file(save_path, loaded_scenario, loaded_world);

  require(loaded_scenario.stage == 0, "Loaded stage 0");
  require(loaded_scenario.room == 2, "Loaded room 2 (graveyard)");
  require(loaded_scenario.player_health == 180, "Loaded health 180");
  require(loaded_scenario.flags[3][1] == 0x40000000u, "Loaded defeat flag in room 2");

  fs::remove_all(temp_dir);
}

void test_decode_transaction() {
  // Valid checksum and length header around a short payload: parsing must
  // fail without publishing any of the partially decoded coordinates.
  Bytes bytes(20, 0);
  auto put = [&](size_t at, uint32_t word) {
    for (size_t i = 0; i < 4; ++i) bytes[at + i] = uint8_t(word >> (i * 8));
  };
  put(0, SAVE_MAGIC);
  put(4, SAVE_VERSION);
  put(8, 4);
  bytes[16] = 2;
  bytes[17] = 9;
  uint32_t crc = 0xffffffffu;
  for (size_t i = 16; i < bytes.size(); ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1));
  }
  put(12, ~crc);
  ScenarioState state;
  state.stage = 1;
  state.room = 8;
  state.inventory[0] = 3;
  PlayerWorldState world;
  world.player.position = {7, 8, 9};
  bool rejected = false;
  try { deserialize_save_state(bytes, state, world); }
  catch (const std::runtime_error &) { rejected = true; }
  require(rejected, "Short checksummed payload rejected");
  require(state.stage == 1 && state.room == 8 && state.inventory[0] == 3,
          "Failed decode retained inventory and scenario");
  require(world.player.position.x == 7, "Failed decode retained world");
}

} // namespace

int main() {
  try {
    test_round_trip();
    for (const auto pending : {0x10u, 0x400u, 0x800u}) {
      RoomRuntime pending_runtime;
      pending_runtime.scenario.flags[10][0] = pending;
      bool rejected = false;
      try { (void)serialize_save_state(pending_runtime); } catch (const std::exception &) { rejected = true; }
      require(rejected, "Pending inventory/world transaction cannot be saved");
    }
    test_decode_transaction();
    test_corrupted_data();
    test_truncated_data();
    test_magic_mismatch();
    test_version_mismatch();
    test_invalid_inventory_owner();
    test_file_io();
    std::cout << "save_state_tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAILED: " << e.what() << '\n';
    return 1;
  }
}
