#include "audio_assets.hpp"
#include "audio_disk.hpp"
#include <algorithm>
#include <stdexcept>
namespace ashford {
using namespace audio_detail;
// Adapted disk semantics from recvx-decomp sdfunc.c/ps2_sg_sd.c (MIT).
size_t sound_pack_index(const Bytes &index, uint16_t key, size_t entry_count) {
  if (index.size() != 2048 || key == 0xffff)
    throw std::runtime_error("Invalid sound pack index or key");
  Disk disk(index);
  for (size_t i = 0; i < 1024; ++i) {
    const auto candidate = disk.u16(i * 2);
    if (candidate == 0xffff)
      break;
    if (candidate == key) {
      if (i >= entry_count)
        throw std::runtime_error("Sound pack index exceeds archive");
      return i;
    }
  }
  throw std::runtime_error("Sound pack key not found");
}
std::vector<SoundPackEntry> sound_pack_entries(const Bytes &data) {
  Disk disk(data);
  std::vector<SoundPackEntry> entries;
  size_t header_end = 0, payload_start = data.size();
  for (size_t at = 0;; at += 16) {
    if (at >= payload_start || entries.size() >= 4096)
      throw std::runtime_error("Sound pack header lacks terminator");
    disk.range(at, 16);
    if (!disk.u32(at)) {
      header_end = at + 16;
      break;
    }
    SoundPackEntry entry{disk.u32(at), disk.u32(at + 4), disk.u32(at + 8),
                         disk.u32(at + 12)};
    if (!entry.size || entry.type > 5 || (entry.type != 5 && entry.bank > 15))
      throw std::runtime_error("Unsupported sound pack record");
    disk.range(entry.offset, entry.size);
    payload_start = std::min(payload_start, size_t(entry.offset));
    entries.push_back(entry);
  }
  if (entries.empty() || payload_start < header_end)
    throw std::runtime_error("Sound pack payload overlaps header");
  auto sorted = entries;
  std::sort(sorted.begin(), sorted.end(),
            [](auto a, auto b) { return a.offset < b.offset; });
  size_t end = header_end;
  for (const auto &entry : sorted) {
    if (entry.offset < end)
      throw std::runtime_error("Overlapping sound pack payloads");
    end = size_t(entry.offset) + entry.size;
  }
  return entries;
}
// Distinct native adaptation informed by VGMTrans SonyPS2 and vgmstream HD/BD.
// Full parameter bytes are retained for synthesis; no field is silently
// applied.
SceSoundBank decode_sce_sound_bank(const Bytes &data, size_t frame_limit) {
  frame_limit = std::min(frame_limit, sound_pcm_frame_limit);
  Disk wrapper(data);
  wrapper.range(0, 16);
  const size_t ho = wrapper.u32(0), hs = wrapper.u32(4), bo = wrapper.u32(8),
               bs = wrapper.u32(12);
  if (ho < 16 || bo < 16 || !hs || !bs)
    throw std::runtime_error("Invalid sound bank wrapper");
  const auto transported_hd = wrapper.view(ho, hs);
  const auto transported_bd = wrapper.view(bo, bs);
  if (ho < bo + bs && bo < ho + hs)
    throw std::runtime_error("Overlapping sound bank wrapper extents");
  transported_hd.tag(0, "IECSsreV");
  if (transported_hd.u32(8) != 16 || transported_hd.u32(12) != 0x01010000)
    throw std::runtime_error("Unsupported SCEI instrument version");
  const auto head = chunk(transported_hd, 16, "IECSdaeH");
  if (head.size() != 64)
    throw std::runtime_error("Unsupported SCEI instrument header");
  const size_t header_size = head.u32(12), body_size = head.u32(16);
  if (header_size < 80 || header_size > hs || hs - header_size >= 64 ||
      !body_size || body_size > bs || bs - body_size >= 64 || body_size % 16)
    throw std::runtime_error("Invalid SCEI declared bank sizes");
  const auto hd = transported_hd.view(0, header_size);
  const auto bd = transported_bd.view(0, body_size);
  auto bank_chunk = [&](size_t field, const char *signature) {
    const auto at = head.u32(field);
    if (at < 80)
      throw std::runtime_error("SCEI chunk overlaps header");
    return chunk(hd, at, signature);
  };
  const auto vagi = bank_chunk(32, "IECSigaV"),
             smpl = bank_chunk(28, "IECSlpmS"),
             sset = bank_chunk(24, "IECStesS"),
             prog = bank_chunk(20, "IECSgorP");
  SceSoundBank result;
  result.header = hd.copy(0, hd.size());
  result.body = bd.copy(0, bd.size());
  const size_t waves = table_count(vagi);
  size_t remaining_frames = frame_limit;
  for (size_t i = 0; i < waves; ++i) {
    const auto at = parameter(vagi, waves, i, 8);
    if (at == 0xffffffffu)
      throw std::runtime_error("SCEI wave parameter missing");
    SceWave wave;
    wave.body_offset = vagi.u32(at);
    wave.sample_rate = vagi.u16(at + 4);
    wave.attribute = vagi.u8(at + 6);
    if (!wave.sample_rate || wave.attribute > 1 ||
        wave.body_offset >= body_size || wave.body_offset % 16 ||
        (vagi.u8(at + 7) != 0 && vagi.u8(at + 7) != 0xff))
      throw std::runtime_error("Invalid SCEI wave parameters");
    if (!result.waves.empty() &&
        wave.body_offset < result.waves.back().body_offset)
      throw std::runtime_error("Unordered SCEI wave offsets");
    result.waves.push_back(std::move(wave));
  }
  for (size_t i = 0; i < waves; ++i) {
    auto &wave = result.waves[i];
    size_t end = body_size;
    for (size_t j = i + 1; j < waves; ++j)
      if (result.waves[j].body_offset > wave.body_offset) {
        end = result.waves[j].body_offset;
        break;
      }
    wave.body_size = uint32_t(end - wave.body_offset);
    if (remaining_frames < 28)
      throw std::runtime_error("SCEI bank PCM expansion budget exceeded");
    wave.sample = decode_ps_adpcm(bd.copy(wave.body_offset, wave.body_size),
                                  remaining_frames);
    remaining_frames -= wave.sample.pcm.size();
    if (wave.sample.pcm.empty() || (wave.attribute && !wave.sample.loop))
      throw std::runtime_error("SCEI wave has no playable encoded extent");
    if (!wave.attribute)
      wave.sample.loop.reset();
  }
  const size_t layers = table_count(smpl);
  result.layers.resize(layers);
  for (size_t i = 0; i < layers; ++i) {
    const auto at = parameter(smpl, layers, i, 42);
    if (at == 0xffffffffu)
      continue;
    SceSampleLayer layer;
    layer.parameters = smpl.copy(at, 42);
    layer.wave = smpl.u16(at);
    layer.velocity_low = smpl.u8(at + 2);
    layer.velocity_high = smpl.u8(at + 4);
    layer.base_note = smpl.u8(at + 11);
    layer.volume = smpl.u8(at + 16);
    layer.adsr1 = smpl.u16(at + 18);
    layer.adsr2 = smpl.u16(at + 20);
    seven_bit(layer.velocity_low);
    seven_bit(layer.velocity_high);
    seven_bit(layer.base_note);
    seven_bit(layer.volume);
    if (layer.wave >= waves || layer.velocity_low > layer.velocity_high)
      throw std::runtime_error(
          "Invalid SCEI sample layer reference or velocity");
    result.layers[i] = std::move(layer);
  }
  const size_t sets = table_count(sset);
  result.sample_sets.resize(sets);
  for (size_t i = 0; i < sets; ++i) {
    const auto at = parameter(sset, sets, i, 4);
    if (at == 0xffffffffu)
      continue;
    SceSampleSet set;
    set.parameters = sset.copy(at, 4);
    seven_bit(set.parameters[1]);
    seven_bit(set.parameters[2]);
    if (set.parameters[1] > set.parameters[2])
      throw std::runtime_error("Invalid SCEI sample set velocity");
    const size_t count = set.parameters[3];
    sset.range(at + 4, count * 2);
    for (size_t j = 0; j < count; ++j) {
      const auto layer = sset.u16(at + 4 + j * 2);
      if (layer >= layers || !result.layers[layer])
        throw std::runtime_error("Missing SCEI sample set layer");
      set.layers.push_back(layer);
    }
    result.sample_sets[i] = std::move(set);
  }
  const size_t programs = table_count(prog);
  result.programs.resize(programs);
  for (size_t i = 0; i < programs; ++i) {
    const auto at = parameter(prog, programs, i, 36);
    if (at == 0xffffffffu)
      continue;
    SceProgram program;
    program.parameters = prog.copy(at, 36);
    program.volume = prog.u8(at + 6);
    seven_bit(program.volume);
    const size_t count = prog.u8(at + 4), relative = prog.u32(at);
    if (prog.u8(at + 5) != 20 || relative < 36 || relative > prog.size() - at)
      throw std::runtime_error("Unsupported SCEI split block layout");
    const auto start = at + relative;
    prog.range(start, count * 20);
    for (size_t j = 0; j < count; ++j) {
      const auto split_at = start + j * 20;
      SceSplit split;
      split.parameters = prog.copy(split_at, 20);
      split.sample_set = prog.u16(split_at);
      split.note_low = prog.u8(split_at + 2);
      split.note_high = prog.u8(split_at + 4);
      split.volume = prog.u8(split_at + 16);
      seven_bit(split.note_low);
      seven_bit(split.note_high);
      seven_bit(split.volume);
      if (split.sample_set >= sets || !result.sample_sets[split.sample_set])
        throw std::runtime_error("Missing SCEI split sample set");
      program.splits.push_back(std::move(split));
    }
    result.programs[i] = std::move(program);
  }
  return result;
}
} // namespace ashford
