#include "audio_assets.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
namespace {
Bytes read(std::ifstream &stream, uint64_t offset, size_t size) {
  if (size > 64 * 1024 * 1024)
    throw std::runtime_error("Sound archive extent exceeds limit");
  Bytes bytes(size);
  stream.seekg(std::streamoff(offset));
  if (!stream.read(reinterpret_cast<char *>(bytes.data()),
                   std::streamsize(size)))
    throw std::runtime_error("Cannot read sound archive extent");
  return bytes;
}
uint32_t u32(const Bytes &bytes, size_t at) {
  if (at > bytes.size() || bytes.size() - at < 4)
    throw std::runtime_error("Short sound header");
  return uint32_t(bytes[at]) | uint32_t(bytes[at + 1]) << 8 |
         uint32_t(bytes[at + 2]) << 16 | uint32_t(bytes[at + 3]) << 24;
}
void integer(std::ostream &out, uint32_t value, size_t width = 4) {
  for (size_t i = 0; i < width; ++i)
    out.put(char(uint8_t(value >> (i * 8))));
}
void write_bytes(const std::filesystem::path &path, const Bytes &data) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(data.data()),
            std::streamsize(data.size()));
  if (!out)
    throw std::runtime_error("Cannot write sound diagnostic");
}
void write_wav(const std::filesystem::path &path, const SceWave &wave) {
  std::ofstream out(path, std::ios::binary);
  const uint32_t size = uint32_t(wave.sample.pcm.size() * 2);
  const uint32_t extra = wave.sample.loop ? 68 : 0;
  out.write("RIFF", 4);
  integer(out, 36 + size + extra);
  out.write("WAVEfmt ", 8);
  integer(out, 16);
  integer(out, 1, 2);
  integer(out, 1, 2);
  integer(out, wave.sample_rate);
  integer(out, wave.sample_rate * 2);
  integer(out, 2, 2);
  integer(out, 16, 2);
  out.write("data", 4);
  integer(out, size);
  for (auto value : wave.sample.pcm)
    integer(out, uint16_t(value), 2);
  if (wave.sample.loop) {
    out.write("smpl", 4);
    integer(out, 60);
    integer(out, 0);
    integer(out, 0);
    integer(out, 1000000000u / wave.sample_rate);
    integer(out, 60);
    integer(out, 0);
    integer(out, 0);
    integer(out, 0);
    integer(out, 1);
    integer(out, 0);
    integer(out, 0);
    integer(out, 0);
    integer(out, uint32_t(wave.sample.loop->first));
    integer(out, uint32_t(wave.sample.loop->second - 1));
    integer(out, 0);
    integer(out, 0);
  }
  if (!out)
    throw std::runtime_error("Cannot write decoded WAV");
}
const char *event_name(SceEventType type) {
  switch (type) {
  case SceEventType::note_on:
    return "note_on";
  case SceEventType::note_off:
    return "note_off";
  case SceEventType::control:
    return "control";
  case SceEventType::program:
    return "program";
  case SceEventType::pitch:
    return "pitch";
  case SceEventType::tempo:
    return "tempo";
  case SceEventType::end:
    return "end";
  }
  throw std::runtime_error("Invalid decoded sound event");
}
struct Record {
  SoundPackEntry entry;
  std::optional<SceSoundBank> instruments;
  std::optional<std::vector<std::optional<SceSequence>>> sequences;
};
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    if (argc != 5)
      throw std::runtime_error("Usage: AshfordAudioInspect MULTSPQ.AFS "
                               "MULTSPQ.IDX room-key new-output-directory");
    size_t consumed = 0;
    const auto key = std::stoul(argv[3], &consumed);
    if (consumed != std::wstring(argv[3]).size() || key >= 0xffff)
      throw std::runtime_error("Invalid sound pack key");
    const std::filesystem::path source(argv[1]), index_path(argv[2]),
        destination(argv[4]);
    if (std::filesystem::exists(destination))
      throw std::runtime_error(
          "Sound diagnostic output directory already exists");
    std::ifstream afs(source, std::ios::binary),
        idx(index_path, std::ios::binary);
    if (!afs || !idx)
      throw std::runtime_error("Cannot open sound archive/index");
    const auto first = read(afs, 0, 8);
    const size_t count = u32(first, 4);
    if (!count || count > 10000)
      throw std::runtime_error("Invalid sound archive count");
    const auto entries = afs_index(read(afs, 0, 8 + count * 8),
                                   std::filesystem::file_size(source));
    const auto selected = entries[sound_pack_index(
        read(idx, 0, size_t(std::filesystem::file_size(index_path))),
        uint16_t(key), entries.size())];
    const auto pack = read(afs, selected.offset, selected.size);
    std::vector<Record> records;
    for (auto entry : sound_pack_entries(pack)) {
      const Bytes blob(pack.begin() + entry.offset,
                       pack.begin() + entry.offset + entry.size);
      Record record;
      record.entry = entry;
      if (entry.type == 1 || entry.type == 2)
        record.instruments = decode_sce_sound_bank(blob);
      else if (entry.type == 0)
        record.sequences = decode_sce_sequences(blob);
      else if (entry.type != 5)
        throw std::runtime_error("Unsupported sound diagnostic bank type");
      records.push_back(std::move(record));
    }
    // All assets decode before creating output. Never overwrite prior evidence.
    if (!std::filesystem::create_directory(destination))
      throw std::runtime_error(
          "Cannot create fresh sound diagnostic directory");
    std::ofstream samples(destination / "samples.tsv"),
        sequences(destination / "sequences.tsv");
    samples << "record\ttype\tbank\twave\trate\tframes\tloop_start\tloop_"
               "end\twav\theader\tbody\n";
    sequences << "record\tbank\tsequence\tppqn\ttick\ttype\tchannel\tdata1\tdat"
                 "a2\ttempo\n";
    size_t wave_count = 0, event_count = 0;
    for (size_t i = 0; i < records.size(); ++i) {
      const auto &record = records[i];
      const auto prefix = "r" + std::to_string(i) + "_t" +
                          std::to_string(record.entry.type) + "_b" +
                          std::to_string(record.entry.bank);
      if (record.instruments) {
        const auto &bank = *record.instruments;
        write_bytes(destination / (prefix + ".hd"), bank.header);
        write_bytes(destination / (prefix + ".bd"), bank.body);
        for (size_t j = 0; j < bank.waves.size(); ++j) {
          const auto &wave = bank.waves[j];
          const auto name = prefix + "_w" + std::to_string(j) + ".wav";
          write_wav(destination / name, wave);
          samples << i << '\t' << record.entry.type << '\t' << record.entry.bank
                  << '\t' << j << '\t' << wave.sample_rate << '\t'
                  << wave.sample.pcm.size() << '\t';
          if (wave.sample.loop)
            samples << wave.sample.loop->first;
          samples << '\t';
          if (wave.sample.loop)
            samples << wave.sample.loop->second;
          samples << '\t' << name << '\t' << prefix << ".hd\t" << prefix
                  << ".bd\n";
          ++wave_count;
        }
      }
      if (record.sequences)
        for (size_t j = 0; j < record.sequences->size(); ++j) {
          const auto &sequence = (*record.sequences)[j];
          if (!sequence)
            continue;
          for (const auto &event : sequence->events) {
            sequences << i << '\t' << record.entry.bank << '\t' << j << '\t'
                      << sequence->ppqn << '\t' << event.tick << '\t'
                      << event_name(event.type) << '\t'
                      << unsigned(event.channel) << '\t'
                      << unsigned(event.data1) << '\t' << unsigned(event.data2)
                      << '\t' << event.tempo << '\n';
            ++event_count;
          }
        }
    }
    samples.flush();
    sequences.flush();
    if (!samples || !sequences)
      throw std::runtime_error("Cannot write sound manifests");
    std::cout << "Native sound key=" << key << " waves=" << wave_count
              << " events=" << event_count << " (no audio device)\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
