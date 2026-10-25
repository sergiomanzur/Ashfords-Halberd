#include "save_state.hpp"
#include "item_database.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace ashford {

namespace {

uint32_t calculate_crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j) {
      crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
  }
  return ~crc;
}

struct BufferWriter {
  Bytes &b;
  void u8(uint8_t v) { b.push_back(v); }
  void u16(uint16_t v) {
    b.push_back(uint8_t(v & 0xff));
    b.push_back(uint8_t((v >> 8) & 0xff));
  }
  void u32(uint32_t v) {
    for (int i = 0; i < 4; ++i)
      b.push_back(uint8_t((v >> (i * 8)) & 0xff));
  }
  void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
  void flt(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    u32(bits);
  }
};

struct BufferReader {
  const Bytes &b;
  size_t p = 0;
  void require(size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Save state buffer truncated");
  }
  uint8_t u8() {
    require(1);
    return b[p++];
  }
  uint16_t u16() {
    require(2);
    uint16_t v = uint16_t(b[p]) | (uint16_t(b[p + 1]) << 8);
    p += 2;
    return v;
  }
  uint32_t u32() {
    require(4);
    uint32_t v = uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) |
                 (uint32_t(b[p + 2]) << 16) | (uint32_t(b[p + 3]) << 24);
    p += 4;
    return v;
  }
  int32_t i32() { return static_cast<int32_t>(u32()); }
  float flt() {
    uint32_t bits = u32();
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
  }
};

} // namespace

Bytes serialize_save_state(const RoomRuntime &runtime) {
  if (runtime.scenario.flags[10][0] & (0x10u | 0x400u | 0x800u))
    throw std::runtime_error("Finish the pending item interaction before saving");
  Bytes payload;
  BufferWriter w{payload};

  // 1. Coordinates and navigation
  w.u8(runtime.scenario.stage);
  w.u8(runtime.scenario.room);
  w.u8(runtime.scenario.floor_index);
  w.u8(runtime.world.floor);
  w.flt(runtime.world.player.position.x);
  w.flt(runtime.world.player.position.y);
  w.flt(runtime.world.player.position.z);
  w.u16(runtime.world.player.yaw);
  w.u16(runtime.world.player.motion);
  w.i32(runtime.world.player.frame);

  // 2. Player status & inventory
  w.i32(runtime.scenario.player_health);
  w.i32(runtime.scenario.equipped_weapon);
  w.u32(runtime.scenario.player_work_flags);
  w.u32(runtime.scenario.player_state_flags);
  w.u32(runtime.scenario.player_secondary_flags);

  // 384 inventory words
  for (size_t i = 0; i < runtime.scenario.inventory.size(); ++i) {
    w.u32(runtime.scenario.inventory[i]);
  }

  // 3. Persistent scenario flags (17 banks of 32 words)
  for (size_t bank = 0; bank < runtime.scenario.flags.size(); ++bank) {
    for (size_t word = 0; word < runtime.scenario.flags[bank].size(); ++word) {
      w.u32(runtime.scenario.flags[bank][word]);
    }
  }

  // These select inventory ownership, recovery/pickup rules and room resources.
  w.u8(runtime.scenario.player_id);
  w.i32(runtime.scenario.game_mode);
  w.i32(runtime.scenario.room_case);
  w.i32(runtime.scenario.room_sound_case);
  w.u32(runtime.scenario.spray_use_count);

  // Build header
  SaveHeader hdr;
  hdr.magic = SAVE_MAGIC;
  hdr.version = SAVE_VERSION;
  hdr.payload_size = static_cast<uint32_t>(payload.size());
  hdr.checksum = calculate_crc32(payload.data(), payload.size());

  Bytes result;
  BufferWriter hw{result};
  hw.u32(hdr.magic);
  hw.u32(hdr.version);
  hw.u32(hdr.payload_size);
  hw.u32(hdr.checksum);
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

void deserialize_save_state(const Bytes &data, ScenarioState &target_scenario,
                            PlayerWorldState &target_world) {
  if (data.size() < 16)
    throw std::runtime_error("Save data smaller than header");

  BufferReader r{data};
  uint32_t magic = r.u32();
  if (magic != SAVE_MAGIC)
    throw std::runtime_error("Invalid save file magic");

  uint32_t version = r.u32();
  if (version != SAVE_VERSION)
    throw std::runtime_error("Unsupported save file version");

  uint32_t payload_size = r.u32();
  uint32_t checksum = r.u32();

  if (data.size() - 16 != payload_size)
    throw std::runtime_error("Save payload size mismatch");

  uint32_t actual_crc = calculate_crc32(data.data() + 16, payload_size);
  if (actual_crc != checksum)
    throw std::runtime_error("Save checksum corrupted");

  // Decode into private replacements. A valid CRC alone does not establish
  // a complete or semantically valid payload.
  auto out_scenario = target_scenario;
  auto out_world = target_world;
  out_scenario.stage = r.u8();
  out_scenario.room = r.u8();
  out_scenario.floor_index = r.u8();
  out_world.floor = r.u8();
  out_world.player.position.x = r.flt();
  out_world.player.position.y = r.flt();
  out_world.player.position.z = r.flt();
  out_world.player.yaw = r.u16();
  out_world.player.motion = r.u16();
  out_world.player.frame = r.i32();

  out_scenario.player_health = r.i32();
  out_scenario.equipped_weapon = r.i32();
  out_scenario.player_work_flags = r.u32();
  out_scenario.player_state_flags = r.u32();
  out_scenario.player_secondary_flags = r.u32();

  for (size_t i = 0; i < out_scenario.inventory.size(); ++i) {
    out_scenario.inventory[i] = r.u32();
  }

  for (size_t bank = 0; bank < out_scenario.flags.size(); ++bank) {
    for (size_t word = 0; word < out_scenario.flags[bank].size(); ++word) {
      out_scenario.flags[bank][word] = r.u32();
    }
  }
  out_scenario.player_id = r.u8();
  out_scenario.game_mode = r.i32();
  const auto saved_room_case = r.i32();
  if (saved_room_case < 0 || saved_room_case > 255)
    throw std::runtime_error("Invalid saved room variant");
  out_scenario.room_case = static_cast<uint8_t>(saved_room_case);
  out_scenario.room_sound_case = r.i32();
  out_scenario.spray_use_count = r.u32();
  if (r.p != data.size())
    throw std::runtime_error("Unexpected save payload fields");
  if (out_scenario.player_id >= 4 || out_scenario.game_mode < 0 ||
      out_scenario.game_mode > 3 ||
      out_scenario.room_sound_case < 0 ||
      out_scenario.inventory[size_t(out_scenario.player_id) * 16] >
          ((out_scenario.flags[11][0] & 0x8000000u) ? 11u : 9u))
    throw std::runtime_error("Invalid saved inventory owner or mode");
  if (out_scenario.flags[10][0] & (0x10u | 0x400u | 0x800u))
    throw std::runtime_error("Saved item interaction is not committed");
  const auto position = out_world.player.position;
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z))
    throw std::runtime_error("Invalid saved player coordinates");
  const size_t bank = size_t(out_scenario.player_id) * 16;
  const auto selector = out_scenario.inventory[bank];
  const int32_t weapon = selector ? ItemDatabase::weapon_index(
      uint16_t(out_scenario.inventory[bank + selector] >> 16)) : 0;
  if (weapon < 0 || weapon != out_scenario.equipped_weapon)
    throw std::runtime_error("Saved weapon does not match its inventory owner");
  target_scenario = std::move(out_scenario);
  target_world = std::move(out_world);
}

void save_game_to_file(const fs::path &path, const RoomRuntime &runtime) {
  fs::create_directories(path.parent_path());
  auto temp_path = path;
  temp_path += ".tmp";

  auto bytes = serialize_save_state(runtime);
  {
    std::ofstream out(temp_path, std::ios::binary);
    if (!out)
      throw std::runtime_error("Failed to open save file for writing");
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out)
      throw std::runtime_error("Failed to write save state bytes");
  }

  std::error_code ec;
  fs::rename(temp_path, path, ec);
  if (ec) {
    fs::remove(temp_path, ec);
    throw std::runtime_error("Atomic save file commit failed");
  }
}

void load_game_from_file(const fs::path &path, ScenarioState &out_scenario,
                         PlayerWorldState &out_world) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error("Failed to open save file");

  in.seekg(0, std::ios::end);
  auto size = in.tellg();
  in.seekg(0, std::ios::beg);

  if (size < 16 || size > 16 * 1024 * 1024)
    throw std::runtime_error("Invalid save file length");

  Bytes bytes(static_cast<size_t>(size));
  if (!in.read(reinterpret_cast<char *>(bytes.data()), size))
    throw std::runtime_error("Failed to read save file");

  deserialize_save_state(bytes, out_scenario, out_world);
}

} // namespace ashford
