#include "audio_assets.hpp"
#include "audio_room.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
namespace {
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
template <class F> void rejects(F f, const char *message) {
  bool caught = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  check(caught, message);
}
void put(Bytes &b, size_t at, uint32_t value, size_t width = 4) {
  for (size_t i = 0; i < width; ++i)
    b.at(at + i) = uint8_t(value >> (i * 8));
}
void tag(Bytes &b, size_t at, const char *value) {
  for (size_t i = 0; i < 8; ++i)
    b.at(at + i) = uint8_t(value[i]);
}
void chunk(Bytes &b, size_t at, const char *value, uint32_t size,
           uint32_t maximum) {
  tag(b, at, value);
  put(b, at + 8, size);
  put(b, at + 12, maximum);
}
void pack_tests() {
  Bytes index(2048, 0xff);
  put(index, 0, 10, 2);
  put(index, 2, 20, 2);
  put(index, 4, 21, 2);
  check(sound_pack_index(index, 21, 3) == 2, "SPQ key must use IDX ordering");
  rejects([&] { sound_pack_index(index, 21, 2); }, "SPQ missing archive entry");
  rejects([&] { sound_pack_index(index, 22, 3); }, "SPQ unknown key");
  rejects([&] { sound_pack_index(index, 0xffff, 3); },
          "SPQ sentinel is not a key");
  index.resize(2047);
  rejects([&] { sound_pack_index(index, 10, 3); }, "SPQ truncated IDX");
  Bytes pack(96);
  put(pack, 0, 48);
  put(pack, 4, 16);
  put(pack, 8, 0);
  put(pack, 12, 3);
  put(pack, 16, 64);
  put(pack, 20, 32);
  put(pack, 24, 5);
  put(pack, 28, 0xcccccccc);
  auto entries = sound_pack_entries(pack);
  check(entries.size() == 2 && entries[0].bank == 3 && entries[1].type == 5,
        "SPQ records must include environment and preserve bank");
  put(pack, 0, 32);
  rejects([&] { sound_pack_entries(pack); }, "SPQ header overlap");
  put(pack, 0, 48);
  put(pack, 16, 56);
  rejects([&] { sound_pack_entries(pack); }, "SPQ overlapping blobs");
  put(pack, 16, 64);
  put(pack, 4, 100);
  rejects([&] { sound_pack_entries(pack); }, "SPQ extent overflow");
  put(pack, 4, 16);
  put(pack, 8, 6);
  rejects([&] { sound_pack_entries(pack); }, "SPQ unknown type");
  put(pack, 8, 0);
  put(pack, 12, 16);
  rejects([&] { sound_pack_entries(pack); }, "SPQ invalid bank");
  put(pack, 12, 3);
  put(pack, 32, 64);
  rejects([&] { sound_pack_entries(pack); },
          "SPQ missing terminator before payload");
}
Bytes bank_fixture() {
  Bytes h(400);
  chunk(h, 0, "IECSsreV", 16, 0x01010000);
  chunk(h, 16, "IECSdaeH", 64, 400);
  put(h, 32, 32);
  put(h, 36, 288);
  put(h, 40, 240);
  put(h, 44, 128);
  put(h, 48, 80);
  put(h, 52, 0xffffffff);
  chunk(h, 80, "IECSigaV", 48, 1);
  put(h, 96, 24);
  put(h, 100, 32);
  put(h, 104, 0);
  put(h, 108, 32000, 2);
  h[111] = 0xff;
  put(h, 112, 16);
  put(h, 116, 22050, 2);
  h[118] = 1;
  h[119] = 0xff;
  chunk(h, 128, "IECSlpmS", 112, 1);
  put(h, 144, 24);
  put(h, 148, 66);
  for (size_t at : {size_t(152), size_t(194)}) {
    h[at + 2] = 1;
    h[at + 4] = 127;
    h[at + 11] = 60;
    h[at + 13] = 64;
    h[at + 16] = 127;
    put(h, at + 18, 0x80ff, 2);
    put(h, at + 20, 0x5fe6, 2);
  }
  put(h, 194, 1, 2);
  h[205] = 48;
  h[210] = 100;
  chunk(h, 240, "IECStesS", 48, 1);
  put(h, 256, 24);
  put(h, 260, 30);
  for (size_t at : {size_t(264), size_t(270)}) {
    h[at + 1] = 1;
    h[at + 2] = 127;
    h[at + 3] = 1;
  }
  put(h, 268, 0, 2);
  put(h, 274, 1, 2);
  chunk(h, 288, "IECSgorP", 112, 1);
  put(h, 304, 0xffffffff);
  put(h, 308, 24);
  put(h, 312, 36);
  h[316] = 1;
  h[317] = 20;
  h[318] = 90;
  h[319] = 64;
  put(h, 348, 1, 2);
  h[352] = 127;
  h[364] = 127;
  h[365] = 64;
  Bytes wrapped(528);
  put(wrapped, 0, 16);
  put(wrapped, 4, 448);
  put(wrapped, 8, 464);
  put(wrapped, 12, 64);
  std::copy(h.begin(), h.end(), wrapped.begin() + 16);
  wrapped[464] = 0x0c;
  wrapped[465] = 1;
  wrapped[466] = 0x21;
  wrapped[480] = 0x0c;
  wrapped[481] = 3;
  wrapped[482] = 0x43;
  return wrapped;
}
void bank_tests() {
  auto b = bank_fixture();
  auto bank = decode_sce_sound_bank(b);
  check(bank.waves.size() == 2 && bank.waves[1].sample_rate == 22050,
        "SCEI must decode both indexed waves");
  check(bank.header.size() == 400 && bank.body.size() == 32,
        "SCEI declared sizes exclude transport padding");
  check(bank.waves[0].sample.pcm[0] == 1 && bank.waves[1].sample.pcm[1] == 4 &&
            bank.waves[1].sample.loop == std::make_pair(size_t(0), size_t(28)),
        "SCEI PCM and loop binding");
  check(bank.programs.size() == 2 && !bank.programs[0] &&
            bank.programs[1]->volume == 90 &&
            bank.programs[1]->splits[0].sample_set == 1,
        "SCEI sparse program identity");
  check(bank.sample_sets[1]->layers == std::vector<uint16_t>{1} &&
            bank.layers[1]->wave == 1 && bank.layers[1]->base_note == 48 &&
            bank.layers[1]->volume == 100 && bank.layers[1]->adsr1 == 0x80ff &&
            bank.layers[1]->adsr2 == 0x5fe6 &&
            bank.layers[1]->parameters.size() == 42,
        "SCEI program/sample/ADSR mapping");
  auto bad = b;
  put(bad, 16 + 194, 2, 2);
  rejects([&] { decode_sce_sound_bank(bad); }, "SCEI escaping wave reference");
  bad = b;
  put(bad, 16 + 274, 2, 2);
  rejects([&] { decode_sce_sound_bank(bad); }, "SCEI escaping layer reference");
  bad = b;
  put(bad, 16 + 348, 2, 2);
  rejects([&] { decode_sce_sound_bank(bad); }, "SCEI escaping set reference");
  bad = b;
  put(bad, 16 + 108, 0, 2);
  rejects([&] { decode_sce_sound_bank(bad); }, "SCEI zero sample rate");
  bad = b;
  put(bad, 16 + 96, 0xffffffff);
  rejects([&] { decode_sce_sound_bank(bad); },
          "SCEI invalid wave parameter pointer");
  bad = b;
  put(bad, 16 + 88, 0xffffffff);
  rejects([&] { decode_sce_sound_bank(bad); }, "SCEI escaping chunk size");
  bad = b;
  put(bad, 16 + 32, 65);
  rejects([&] { decode_sce_sound_bank(bad); },
          "SCEI body declaration exceeds wrapper");
  bad = b;
  put(bad, 8, 32);
  rejects([&] { decode_sce_sound_bank(bad); }, "SCEI overlapping HD/BD");
  bad = b;
  bad.resize(527);
  rejects([&] { decode_sce_sound_bank(bad); }, "SCEI truncated wrapper");
  bad = b;
  bad[16 + 317] = 19;
  rejects([&] { decode_sce_sound_bank(bad); }, "SCEI unsupported split size");
  bad = b;
  bad[481] = 1;
  rejects([&] { decode_sce_sound_bank(bad); },
          "SCEI loop attribute needs encoded loop");
}
Bytes sequence_fixture() {
  // Literal source encoding: last data high bit omits the next delta;
  // source note-off has one data byte, and FF2F has no SMF length byte.
  Bytes events{0,    0xff, 0x51, 3,  7,    0xa1, 0x20, 1,    0xc0, 0x82,
               0xb0, 99,   0x80, 6,  0x80, 0x90, 60,   0xc6, 61,   0x47,
               0x81, 0x00, 0x80, 60, 0,    61,   1,    0xff, 0x2f};
  Bytes second{0, 0x90, 48, 70, 0x83, 0x60, 0x80, 48, 0, 0xff, 0x2f};
  Bytes b(80 + 6 + events.size() + 6 + second.size());
  chunk(b, 0, "IECSsreV", 16, 0x00020000);
  chunk(b, 16, "IECSuqeS", 32, uint32_t(b.size()));
  put(b, 32, 0xffffffff);
  put(b, 36, 48);
  put(b, 40, 0xffffffff);
  put(b, 44, 0xffffffff);
  chunk(b, 48, "IECSidiM", uint32_t(b.size() - 48), 1);
  put(b, 64, 32);
  put(b, 68, uint32_t(38 + events.size()));
  put(b, 80, 6);
  put(b, 84, 480, 2);
  std::copy(events.begin(), events.end(), b.begin() + 86);
  const size_t at = 86 + events.size();
  put(b, at, 6);
  put(b, at + 4, 480, 2);
  std::copy(second.begin(), second.end(), b.begin() + at + 6);
  return b;
}
void sequence_tests() {
  auto b = sequence_fixture();
  auto sequences = decode_sce_sequences(b);
  check(sequences.size() == 2 && sequences[0] && sequences[1],
        "SCEI sequence indices");
  const auto &s = *sequences[0];
  check(s.ppqn == 480 && s.events.size() == 9, "SCEI timed event decoding");
  check(s.events[0].type == SceEventType::tempo && s.events[0].tempo == 500000,
        "SCEI big-endian tempo");
  check(s.events[1].tick == 1 && s.events[1].type == SceEventType::program &&
            s.events[1].data1 == 2,
        "SCEI program event");
  check(s.events[2].tick == 1 && s.events[2].type == SceEventType::control &&
            s.events[2].data1 == 99 && s.events[3].data1 == 6 &&
            s.events[3].tick == 1,
        "SCEI omitted delta preserves loop controls");
  check(s.events[4].tick == 1 && s.events[4].data1 == 60 &&
            s.events[4].data2 == 70 && s.events[5].tick == 1 &&
            s.events[5].data1 == 61 && s.events[5].data2 == 71,
        "SCEI running note-on status");
  check(s.events[6].tick == 129 && s.events[6].type == SceEventType::note_off &&
            s.events[7].tick == 129 && s.events[7].data1 == 61 &&
            s.events[8].tick == 130,
        "SCEI one-byte note-off and running status");
  check(sequences[1]->events[1].tick == 480,
        "SCEI second independent sequence");
  auto bad = b;
  bad[94] = 0xa0;
  rejects([&] { decode_sce_sequences(bad); }, "SCEI unknown channel event");
  bad = b;
  put(bad, 80, 8);
  put(bad, 86, 1, 2);
  rejects([&] { decode_sce_sequences(bad); }, "SCEI unsupported compression");
  bad = b;
  put(bad, 84, 0, 2);
  rejects([&] { decode_sce_sequences(bad); }, "SCEI invalid PPQN");
  bad = b;
  bad[112] = 0x90;
  rejects([&] { decode_sce_sequences(bad); }, "SCEI missing end event");
  bad = b;
  for (size_t p = 86; p < 91; ++p)
    bad[p] = 0x80;
  rejects([&] { decode_sce_sequences(bad); }, "SCEI oversized VLQ");
  bad = b;
  put(bad, 64, 0xfffffffe);
  rejects([&] { decode_sce_sequences(bad); }, "SCEI escaping sequence pointer");
  bad = b;
  put(bad, 64, 0xffffffff);
  auto sparse = decode_sce_sequences(bad);
  check(!sparse[0] && sparse[1], "SCEI sparse sequence indices retained");
}
void adpcm_budget_tests() {
  Bytes blocks(32);
  blocks[0] = 0x0c;
  blocks[16] = 0x0c;
  blocks[17] = 1;
  rejects([&] { decode_ps_adpcm(blocks, 28); },
          "ADPCM must reject PCM budget before expansion");
  check(decode_ps_adpcm(blocks, 56).pcm.size() == 56, "ADPCM exact PCM budget");
}
void bank_budget_tests() {
  auto bank = bank_fixture();
  put(bank, 16 + 100, 24); // Both indices share one wave parameter.
  rejects([&] { decode_sce_sound_bank(bank, 55); },
          "SCEI aliased waves must respect aggregate PCM budget");
  const auto decoded = decode_sce_sound_bank(bank, 56);
  check(decoded.waves.size() == 2 &&
            decoded.waves[0].sample.pcm == decoded.waves[1].sample.pcm,
        "SCEI must retain valid aliased wave indices within budget");
}
void sequence_budget_tests() {
  auto seq = sequence_fixture();
  put(seq, 68, 32); // Both indices share the first nine-event MIDI block.
  rejects([&] { decode_sce_sequences(seq, 17); },
          "SCEI aliased sequences must respect aggregate event budget");
  const auto sequences = decode_sce_sequences(seq, 18);
  check(sequences.size() == 2 && sequences[0]->events.size() == 9 &&
            sequences[1]->events.size() == 9,
        "SCEI must retain valid aliased sequence indices within budget");
}
void room_pack_tests() {
  struct Record {
    uint32_t type, bank;
    Bytes bytes;
  };
  auto pack = [](const std::vector<Record> &records) {
    Bytes out((records.size() + 1) * 16);
    for (size_t i = 0; i < records.size(); ++i) {
      put(out, i * 16, uint32_t(out.size()));
      put(out, i * 16 + 4, uint32_t(records[i].bytes.size()));
      put(out, i * 16 + 8, records[i].type);
      put(out, i * 16 + 12, records[i].bank);
      out.insert(out.end(), records[i].bytes.begin(), records[i].bytes.end());
    }
    return out;
  };
  auto seq = sequence_fixture();
  put(seq, 68, 32);    // Two distinct indices alias the nine-event block.
  Bytes env(64, 0xaa); // Transport padding is not environment fields.
  env[0] = 0xff;
  env[9] = 16;
  env[10] = 0;
  const auto data = pack({{1, 3, bank_fixture()},
                          {2, 3, bank_fixture()},
                          {0, 3, seq},
                          {0, 4, seq},
                          {5, 0xcccccccc, env}});
  const auto decoded = decode_room_sound_pack(data, 112, 36);
  check(decoded.programs[3] && decoded.shots[3] &&
            decoded.programs[3] != decoded.shots[3] && !decoded.programs[2],
        "Room bank/type identities must remain distinct and sparse");
  check(decoded.sequences[3] && decoded.sequences[4] &&
            (*decoded.sequences[3])[1]->events.size() == 9,
        "Room sequence bank and index identity");
  check(decoded.has_environment && decoded.environment[0] == -1 &&
            decoded.environment[9] == 16 && decoded.environment[15] == -86,
        "Room signed sixteen-byte environment excludes transport padding");
  rejects([&] { decode_room_sound_pack(data, 111, 36); },
          "Room aggregate PCM budget must include every bank/type");
  rejects([&] { decode_room_sound_pack(data, 112, 35); },
          "Room aggregate event budget must include all sequence banks");
  rejects(
      [&] {
        decode_room_sound_pack(
            pack({{1, 3, bank_fixture()}, {1, 3, bank_fixture()}}));
      },
      "Room duplicate bank/type must reject rather than replace");
  rejects([&] { decode_room_sound_pack(pack({{0, 3, seq}, {0, 3, seq}})); },
          "Room duplicate sequence bank");
  rejects([&] { decode_room_sound_pack(pack({{5, 0, env}, {5, 1, env}})); },
          "Room duplicate environment ignores its meaningless bank field");
  rejects([&] { decode_room_sound_pack(pack({{5, 0, Bytes(15)}})); },
          "Room truncated environment");
  rejects([&] { decode_room_sound_pack(pack({{5, 0, Bytes(80)}})); },
          "Room excessive environment transport padding");
  rejects([&] { decode_room_sound_pack(pack({{3, 0, Bytes(16)}})); },
          "Room unsupported pack service must reject explicitly");
  check(!decode_room_sound_pack(pack({{1, 0, bank_fixture()}})).has_environment,
        "Missing environment must remain distinguishable from authored zero");
}
void adpcm_tests() {
  Bytes block(16);
  block[0] = 0x0c;
  block[1] = 1;
  block[2] = 0xf1;
  block[3] = 0x87;
  auto sample = decode_ps_adpcm(block);
  check(sample.pcm.size() == 28, "ADPCM must decode 28 samples per block");
  check(sample.pcm[0] == 1 && sample.pcm[1] == -1 && sample.pcm[2] == 7 &&
            sample.pcm[3] == -8,
        "ADPCM signed low-nibble order");
  check(sample.decoded_bytes == 16 && !sample.loop, "ADPCM terminal extent");
  // Literal predictor1 recurrence: floor(60 * previous / 64) + residual.
  block.assign(32, 0);
  block[0] = 0x1c;
  block[2] = 0x44;
  block[16] = 0x1c;
  block[17] = 1;
  block[18] = 0x11;
  sample = decode_ps_adpcm(block);
  check(sample.pcm[0] == 4 && sample.pcm[1] == 7 && sample.pcm[2] == 6 &&
            sample.pcm[7] == 1 && sample.pcm[8] == 0 && sample.pcm[28] == 1,
        "ADPCM predictor history");
  block.assign(48, 0);
  block[0] = 0x0c;
  block[1] = 6;
  block[2] = 0x21;
  block[16] = 0x0c;
  block[17] = 3;
  block[18] = 0x43;
  block[32] = 0;
  block[33] = 7;
  block[34] = 0x77;
  sample = decode_ps_adpcm(block);
  check(sample.pcm.size() == 56 &&
            sample.loop == std::make_pair(size_t(0), size_t(56)),
        "ADPCM loop must exclude terminal filler");
  check(sample.pcm[28] == 3 && sample.pcm[29] == 4,
        "ADPCM loop-end block decoded");
  block[1] = 0;
  block[17] = 6;
  block[33] = 3;
  sample = decode_ps_adpcm(block);
  check(sample.loop == std::make_pair(size_t(28), size_t(84)),
        "ADPCM nonzero loop start");
  block.assign(16, 0x77);
  block[0] = 0x40;
  block[1] = 1;
  sample = decode_ps_adpcm(block);
  check(sample.pcm[0] == 28672 && sample.pcm[1] == 32767, "ADPCM saturation");
  block.assign(16, 0x77);
  block[0] = 0;
  block[1] = 7;
  check(decode_ps_adpcm(block).pcm.empty(), "ADPCM filler must not be sound");
  rejects([] { decode_ps_adpcm({}); }, "ADPCM empty input");
  rejects([] { decode_ps_adpcm(Bytes(15)); }, "ADPCM partial block");
  block[1] = 1;
  block[0] = 0x50;
  rejects([&] { decode_ps_adpcm(block); }, "ADPCM unknown predictor");
  block[0] = 0x0d;
  rejects([&] { decode_ps_adpcm(block); }, "ADPCM invalid shift");
  block[0] = 0;
  block[1] = 8;
  rejects([&] { decode_ps_adpcm(block); }, "ADPCM unknown flags");
  block[1] = 0;
  rejects([&] { decode_ps_adpcm(block); }, "ADPCM missing terminal block");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string(argv[1]) == "write-inspector-fixture") {
      const auto bank = bank_fixture(), seq = sequence_fixture();
      Bytes pack(64 + bank.size() + seq.size());
      put(pack, 0, 64);
      put(pack, 4, uint32_t(bank.size()));
      put(pack, 8, 1);
      put(pack, 12, 3);
      put(pack, 16, uint32_t(64 + bank.size()));
      put(pack, 20, uint32_t(seq.size()));
      put(pack, 24, 0);
      put(pack, 28, 3);
      std::copy(bank.begin(), bank.end(), pack.begin() + 64);
      std::copy(seq.begin(), seq.end(), pack.begin() + 64 + bank.size());
      Bytes afs(32 + pack.size());
      afs[0] = 'A';
      afs[1] = 'F';
      afs[2] = 'S';
      afs[3] = 0x20;
      put(afs, 4, 1);
      put(afs, 8, 32);
      put(afs, 12, uint32_t(pack.size()));
      std::copy(pack.begin(), pack.end(), afs.begin() + 32);
      Bytes idx(2048, 0xff);
      put(idx, 0, 20, 2);
      for (const auto &file : std::vector<std::pair<std::string, Bytes>>{
               {"test.AFS", afs}, {"test.IDX", idx}}) {
        std::ofstream out(std::filesystem::path(argv[2]) / file.first,
                          std::ios::binary);
        out.write(reinterpret_cast<const char *>(file.second.data()),
                  std::streamsize(file.second.size()));
        check(bool(out), "Cannot write synthetic audio fixture");
      }
      return 0;
    }
    const std::string mode = argc == 2 ? argv[1] : "all";
    if (mode == "all" || mode == "adpcm")
      adpcm_tests();
    if (mode == "all" || mode == "pack")
      pack_tests();
    if (mode == "all" || mode == "bank")
      bank_tests();
    if (mode == "all" || mode == "sequence")
      sequence_tests();
    if (mode == "all" || mode == "budget-adpcm")
      adpcm_budget_tests();
    if (mode == "all" || mode == "budget-bank")
      bank_budget_tests();
    if (mode == "all" || mode == "budget-sequence")
      sequence_budget_tests();
    if (mode == "all" || mode == "room-pack")
      room_pack_tests();
    std::cout << "Native sound asset checks passed: " << mode << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
