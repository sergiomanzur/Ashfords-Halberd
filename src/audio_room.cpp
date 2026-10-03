#include "audio_room.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace ashford {
std::shared_ptr<const RoomSoundAssets>
read_room_sound_assets(const std::filesystem::path &archive,
                       const std::filesystem::path &index, uint8_t stage,
                       uint8_t room, int32_t sound_case) {
  const uint32_t key =
      uint32_t(stage) * 1000 + uint32_t(room) * 10 + uint32_t(sound_case);
  if (sound_case < 0 || sound_case > 255 || key >= 0xffff ||
      std::filesystem::file_size(index) != 2048)
    throw std::runtime_error("Invalid native room sound key/index");
  std::ifstream afs(archive, std::ios::binary), idx(index, std::ios::binary);
  auto read = [](std::ifstream &file, uint64_t at, size_t size) {
    if (size > 64 * 1024 * 1024)
      throw std::runtime_error("Room sound extent exceeds limit");
    Bytes bytes(size);
    file.seekg(std::streamoff(at));
    if (!file.read(reinterpret_cast<char *>(bytes.data()),
                   std::streamsize(size)))
      throw std::runtime_error("Could not read native room sound assets");
    return bytes;
  };
  const auto first = read(afs, 0, 8);
  const uint32_t count = uint32_t(first[4]) | uint32_t(first[5]) << 8 |
                         uint32_t(first[6]) << 16 | uint32_t(first[7]) << 24;
  if (count > 10000)
    throw std::runtime_error("Room sound archive count exceeds limit");
  const auto entries = afs_index(read(afs, 0, 8 + size_t(count) * 8),
                                 std::filesystem::file_size(archive));
  const auto entry = entries.at(
      sound_pack_index(read(idx, 0, 2048), uint16_t(key), entries.size()));
  return std::make_shared<const RoomSoundAssets>(
      decode_room_sound_pack(read(afs, entry.offset, entry.size)));
}
RoomSoundAssets decode_room_sound_pack(const Bytes &data, size_t pcm_budget,
                                       size_t event_budget) {
  auto remaining_pcm = std::min(pcm_budget, sound_pcm_frame_limit);
  auto remaining_events = std::min(event_budget, sound_sequence_event_limit);
  RoomSoundAssets result;
  const auto entries = sound_pack_entries(data);
  // Validate identities before any sample/event expansion.
  std::array<std::array<bool, 16>, 3> seen{};
  bool environment = false;
  for (const auto &entry : entries) {
    if (entry.type <= 2) {
      if (seen[entry.type][entry.bank])
        throw std::runtime_error("Duplicate room sound bank/type");
      seen[entry.type][entry.bank] = true;
    } else if (entry.type == 5) {
      if (environment || entry.size < 16 || entry.size - 16 >= 64)
        throw std::runtime_error("Invalid or duplicate room sound environment");
      environment = true;
    } else {
      throw std::runtime_error("Unsupported room sound pack service");
    }
  }
  for (const auto &entry : entries) {
    if (entry.type == 5) {
      for (size_t i = 0; i < 16; ++i) {
        const int value = data[entry.offset + i];
        result.environment[i] = int8_t(value >= 128 ? value - 256 : value);
      }
      result.has_environment = true;
      continue;
    }
    Bytes blob(data.begin() + entry.offset,
               data.begin() + entry.offset + entry.size);
    if (entry.type == 0) {
      auto sequences = decode_sce_sequences(blob, remaining_events);
      for (const auto &sequence : sequences)
        if (sequence)
          remaining_events -= sequence->events.size();
      result.sequences[entry.bank] =
          std::make_shared<const std::vector<std::optional<SceSequence>>>(
              std::move(sequences));
    } else {
      auto bank = decode_sce_sound_bank(blob, remaining_pcm);
      for (const auto &wave : bank.waves)
        remaining_pcm -= wave.sample.pcm.size();
      auto &destination = entry.type == 1 ? result.programs : result.shots;
      destination[entry.bank] =
          std::make_shared<const SceSoundBank>(std::move(bank));
    }
  }
  return result;
}
void MidiVolumeFade::request(bool fade_in, int16_t timer, int16_t limit) {
  if (timer < 0 || limit < -127 || limit > 0)
    throw std::runtime_error("Invalid native MIDI fade timer or volume limit");
  count_ = (((timer / 100) * 30) + (((timer % 100) * 6) / 10)) / 2;
  if (!count_)
    count_ = 1;
  if (fade_in) {
    volume_ = -127.0f;
    last_ = float(limit);
  } else
    last_ = -127.0f;
  speed_ = (volume_ - last_) / float(count_);
  active_ = true;
  stopped_ = false;
}
void MidiVolumeFade::tick() {
  if (!active_)
    return;
  volume_ -= speed_;
  if (--count_ < 0) {
    volume_ = last_;
    stopped_ = int(volume_) == -127;
    active_ = false;
  }
}
int MidiVolumeFade::driver_volume() const {
  return std::clamp(int(volume_) + 128, 0, 127);
}
void RoomEffectDepth::request(int8_t depth) {
  target_ = int32_t(depth) * 256;
  step_ = (target_ - current_) / 16;
  remaining_ = 16;
}
int16_t RoomEffectDepth::tick() {
  if (remaining_)
    current_ = --remaining_ ? current_ + step_ : target_;
  return level();
}
} // namespace ashford
