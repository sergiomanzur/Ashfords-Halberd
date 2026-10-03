#include "audio_assets.hpp"
#include "audio_disk.hpp"
#include <stdexcept>
namespace ashford {
using namespace audio_detail;
namespace {
SceSequence decode_sequence(const Disk &disk, size_t event_limit) {
  const size_t offset = disk.u32(0);
  if (offset != 6 && offset != 8)
    throw std::runtime_error("Unsupported SCEI sequence header");
  if (offset == 8 && disk.u16(6) != 0)
    throw std::runtime_error("Unsupported SCEI sequence compression");
  SceSequence sequence;
  sequence.ppqn = disk.u16(4);
  if (!sequence.ppqn || sequence.ppqn >= 0x8000)
    throw std::runtime_error("Invalid SCEI sequence PPQN");
  size_t cursor = offset;
  uint8_t running = 0;
  bool skip_delta = false;
  uint64_t tick = 0;
  auto byte = [&] { return disk.u8(cursor++); };
  auto data_byte = [&](bool last) {
    const uint8_t value = byte();
    if (last)
      skip_delta = bool(value & 0x80);
    else if (value & 0x80)
      throw std::runtime_error("Invalid SCEI event data byte");
    return uint8_t(value & 0x7f);
  };
  while (cursor < disk.size()) {
    if (sequence.events.size() >= event_limit)
      throw std::runtime_error("SCEI event expansion budget exceeded");
    if (!skip_delta) {
      uint32_t delta = 0;
      bool finished = false;
      for (size_t i = 0; i < 4; ++i) {
        const auto value = byte();
        delta = (delta << 7) | (value & 0x7f);
        if (!(value & 0x80)) {
          finished = true;
          break;
        }
      }
      if (!finished)
        throw std::runtime_error("Invalid SCEI delta VLQ");
      tick += delta;
    }
    skip_delta = false;
    uint8_t status = byte();
    if (status < 0x80) {
      if (!running)
        throw std::runtime_error("SCEI running status missing");
      --cursor;
      status = running;
    } else if (status != 0xff)
      running = status;
    SceEvent event;
    event.tick = tick;
    event.channel = status & 15;
    switch (status & 0xf0) {
    case 0x80:
      event.type = SceEventType::note_off;
      event.data1 = data_byte(true);
      break;
    case 0x90:
      event.data1 = data_byte(false);
      event.data2 = data_byte(true);
      event.type = event.data2 ? SceEventType::note_on : SceEventType::note_off;
      break;
    case 0xb0:
      event.type = SceEventType::control;
      event.data1 = data_byte(false);
      event.data2 = data_byte(true);
      break;
    case 0xc0:
      event.type = SceEventType::program;
      event.data1 = data_byte(true);
      break;
    case 0xe0:
      event.type = SceEventType::pitch;
      event.data1 = data_byte(false);
      event.data2 = data_byte(true);
      break;
    case 0xf0: {
      if (status != 0xff)
        throw std::runtime_error("Unsupported SCEI system event");
      const auto meta = byte();
      event.channel = 0;
      if (meta == 0x2f) {
        event.type = SceEventType::end;
        sequence.events.push_back(event);
        return sequence;
      }
      if (meta != 0x51 || byte() != 3)
        throw std::runtime_error("Unsupported SCEI meta event");
      const auto high = byte(), middle = byte(), low = byte();
      event.type = SceEventType::tempo;
      event.tempo = uint32_t(high) << 16 | uint32_t(middle) << 8 | low;
      if (!event.tempo)
        throw std::runtime_error("Invalid SCEI zero tempo");
      break;
    }
    default:
      throw std::runtime_error("Unsupported SCEI channel event");
    }
    sequence.events.push_back(event);
  }
  throw std::runtime_error("SCEI sequence lacks end event");
}
} // namespace
// Distinct native adaptation of VGMTrans's documented SonyPS2 SQ event format.
std::vector<std::optional<SceSequence>>
decode_sce_sequences(const Bytes &data, size_t event_limit) {
  event_limit = std::min(event_limit, sound_sequence_event_limit);
  Disk transported(data);
  transported.tag(0, "IECSsreV");
  if (transported.u32(8) != 16 ||
      (transported.u32(12) != 0x00010000 && transported.u32(12) != 0x00020000))
    throw std::runtime_error("Unsupported SCEI sequence version");
  const auto header = chunk(transported, 16, "IECSuqeS");
  if (header.size() != 32)
    throw std::runtime_error("Unsupported SCEI sequence file header");
  const size_t declared = header.u32(12);
  if (declared < 48 || declared > transported.size() ||
      transported.size() - declared >= 64)
    throw std::runtime_error("Invalid SCEI sequence file size");
  const auto disk = transported.view(0, declared);
  const size_t midi_at = header.u32(20);
  if (midi_at < 48)
    throw std::runtime_error("SCEI MIDI chunk overlaps header");
  const auto midi = chunk(disk, midi_at, "IECSidiM");
  const size_t count = table_count(midi);
  std::vector<size_t> offsets(count);
  for (size_t i = 0; i < count; ++i)
    offsets[i] = parameter(midi, count, i, 6);
  std::vector<std::optional<SceSequence>> sequences(count);
  for (size_t i = 0; i < count; ++i) {
    const size_t at = offsets[i];
    if (at == 0xffffffffu)
      continue;
    size_t end = midi.size();
    for (auto candidate : offsets)
      if (candidate > at && candidate < end)
        end = candidate;
    sequences[i] = decode_sequence(midi.view(at, end - at), event_limit);
    event_limit -= sequences[i]->events.size();
  }
  return sequences;
}
} // namespace ashford
