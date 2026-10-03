#pragma once
#include "assets.hpp"
#include <optional>
#include <utility>

namespace ashford {
struct PsAdpcmSample {
  std::vector<int16_t> pcm;
  std::optional<std::pair<size_t, size_t>> loop; // Exclusive sample end.
  size_t decoded_bytes = 0;
};
constexpr size_t sound_pcm_frame_limit = 16 * 1024 * 1024;
constexpr size_t sound_sequence_event_limit = 1000000;
PsAdpcmSample decode_ps_adpcm(const Bytes &data,
                              size_t frame_limit = sound_pcm_frame_limit);
struct SoundPackEntry {
  uint32_t offset = 0, size = 0, type = 0, bank = 0;
};
size_t sound_pack_index(const Bytes &index, uint16_t key, size_t entry_count);
std::vector<SoundPackEntry> sound_pack_entries(const Bytes &data);
struct SceWave {
  uint32_t sample_rate = 0, body_offset = 0, body_size = 0;
  uint8_t attribute = 0;
  PsAdpcmSample sample;
};
struct SceSampleLayer {
  Bytes parameters; // Full original 42-byte sample parameter block.
  uint16_t wave = 0, adsr1 = 0, adsr2 = 0;
  uint8_t velocity_low = 0, velocity_high = 0, base_note = 0, volume = 0;
};
struct SceSampleSet {
  Bytes parameters; // Velocity curve and bounds, sample count.
  std::vector<uint16_t> layers;
};
struct SceSplit {
  Bytes parameters; // Full original 20-byte split block.
  uint16_t sample_set = 0;
  uint8_t note_low = 0, note_high = 0, volume = 0;
};
struct SceProgram {
  Bytes parameters; // Full original 36-byte program parameter block.
  uint8_t volume = 0;
  std::vector<SceSplit> splits;
};
struct SceSoundBank {
  Bytes header, body; // Declared sizes only; transport padding is excluded.
  std::vector<SceWave> waves;
  std::vector<std::optional<SceSampleLayer>> layers;
  std::vector<std::optional<SceSampleSet>> sample_sets;
  std::vector<std::optional<SceProgram>> programs;
};
SceSoundBank decode_sce_sound_bank(const Bytes &data,
                                   size_t frame_limit = sound_pcm_frame_limit);
enum class SceEventType {
  note_on,
  note_off,
  control,
  program,
  pitch,
  tempo,
  end
};
struct SceEvent {
  uint64_t tick = 0;
  SceEventType type = SceEventType::end;
  uint8_t channel = 0, data1 = 0, data2 = 0;
  uint32_t tempo = 0;
};
struct SceSequence {
  uint16_t ppqn = 0;
  std::vector<SceEvent> events;
};
std::vector<std::optional<SceSequence>>
decode_sce_sequences(const Bytes &data,
                     size_t event_limit = sound_sequence_event_limit);
} // namespace ashford
