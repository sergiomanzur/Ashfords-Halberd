#include "effect_palette_reference_fixture.hpp"
#include "texture.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void reject(F action, const char *message) {
  bool failed = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  require(failed, message);
}
void word(Bytes &b, size_t p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(value >> (8 * i));
}
void half(Bytes &b, size_t p, uint16_t value) {
  b.at(p) = uint8_t(value);
  b.at(p + 1) = uint8_t(value >> 8);
}
void record(Bytes &b, uint32_t magic, const Bytes &data) {
  const auto p = b.size();
  b.resize(p + 32);
  word(b, p, magic);
  word(b, p + 4, uint32_t(data.size()));
  b.insert(b.end(), data.begin(), data.end());
}
Bytes picture(size_t palettes, uint8_t type, uint8_t delta,
              uint32_t global_index, const Bytes &indices = {}) {
  const size_t image_bytes = type == 4 ? 2 : 4;
  const size_t stride = type == 4 ? 128 : 1024;
  Bytes b(256 + image_bytes + palettes * stride);
  word(b, 0, 0x324d4954);
  b[4] = 4;
  b[5] = 1;
  half(b, 6, 1);
  word(b, 8, global_index);
  b[12] = 6;
  word(b, 128, uint32_t(b.size() - 128));
  word(b, 132, uint32_t(palettes * stride));
  word(b, 136, uint32_t(image_bytes));
  half(b, 140, 128);
  half(b, 142, uint16_t(palettes * (type == 4 ? 16 : 256)));
  b[144] = 5;
  b[146] = 3;
  b[147] = type;
  half(b, 148, 4);
  half(b, 150, 1);
  const Bytes pixels =
      indices.empty() ? (type == 4 ? Bytes{0x80, 0x1f} : Bytes{0, 8, 16, 255})
                      : indices;
  require(pixels.size() == image_bytes, "Invalid test image indices");
  std::copy(pixels.begin(), pixels.end(), b.begin() + 256);
  for (size_t palette = 0; palette < palettes; ++palette)
    for (size_t color = 0; color < stride / 4; ++color) {
      const auto p = 256 + image_bytes + palette * stride + color * 4;
      b[p] = uint8_t(delta + palette * 40 + color);
      b[p + 1] = 70;
      b[p + 2] = 90;
      b[p + 3] = 128;
    }
  return b;
}
Bytes pack(const std::vector<uint32_t> &descriptors, uint8_t type = 4,
           uint8_t delta = 10, uint32_t global_index = 100,
           const Bytes &indices = {}) {
  Bytes metadata(4 + descriptors.size() * 8);
  word(metadata, 0, uint32_t(descriptors.size()));
  for (size_t i = 0; i < descriptors.size(); ++i)
    word(metadata, 8 + i * 8, descriptors[i]);
  Bytes b;
  record(b, 0x00494c50, metadata);
  record(b, 0x324d4954,
         picture(descriptors.size(), type, delta, global_index, indices));
  const auto end = b.size();
  b.resize(end + 4);
  word(b, end, 0xffffffff);
  return b;
}
uint32_t descriptor(unsigned bank, unsigned update = 0, unsigned offset = 0) {
  return uint32_t(update | bank << 8 | offset << 16);
}
void palettes() {
  NativeEffectTextureLibrary library;
  const auto range =
      load_effect_texture_pack(pack({0xff00, 0xff00}), 0, library);
  require(range.first == 0 && range.count == 2 && library.next_bank == 34 &&
              library.textures[0].bank == 32 && library.textures[1].bank == 33,
          "4-bit automatic banks must advance by one");
  const auto image = library.textures[0].image;
  const auto frozen = resolve_effect_texture(library, 0);
  require(frozen.rgba == Bytes({10, 70, 90, 128, 26, 70, 90, 128, 33, 70, 90,
                                128, 11, 70, 90, 128}),
          "Padded CLUT and original half-alpha");
  const auto update =
      load_effect_texture_pack(pack({descriptor(32, 1)}, 4, 110), 0, library);
  require(update.first == 2 && update.count == 0 && library.next_bank == 34 &&
              library.textures[0].image == image && frozen.rgba[0] == 10 &&
              resolve_effect_texture(library, 0).rgba[0] == 110,
          "Cross-pack updates must recolor existing image without emitting");
  load_effect_texture_pack(pack({0xff01}), 0, library);
  require(library.next_bank == 35 && library.textures.size() == 2 &&
              library.palette[34 * 16][0] == 10,
          "Automatic palette-only update must consume its bank");
  load_effect_texture_pack(pack({descriptor(32, 7)}, 4, 120), 0, library);
  require(resolve_effect_texture(library, 0).rgba[0] == 120 &&
              library.textures.size() == 2,
          "Every nonzero TEXflag updates colors without emitting");
  load_effect_texture_pack(pack({descriptor(32, 1, 8)}, 4, 200), 0, library);
  require(resolve_effect_texture(library, 0).rgba[4] == 200 &&
              resolve_effect_texture(library, 1).rgba[0] == 216,
          "Explicit offsets must overlap neighboring palette banks");

  NativeEffectTextureLibrary large;
  load_effect_texture_pack(pack({0xff00, 0xff00}, 5), 0, large);
  require(large.next_bank == 32 && large.textures[0].bank == 32 &&
              large.textures[1].bank == 48 &&
              resolve_effect_texture(large, 0).rgba ==
                  Bytes({10, 70, 90, 128, 26, 70, 90, 128, 18, 70, 90, 128, 9,
                         70, 90, 128}),
          "8-bit banks must advance by16 and select their256-color CLUT");
  load_effect_texture_pack(pack({descriptor(35)}, 5, 140, 200), 0, large);
  const auto partial = resolve_effect_texture(large, 2);
  require(partial.rgba[0] == 10 && partial.rgba[12] == 99 &&
              partial.rgba == resolve_effect_texture(large, 0).rgba &&
              large.palette[35 * 16][0] == 140,
          "8-bit draw ignores lower bank bits while upload uses all bits");

  NativeEffectTextureLibrary high;
  load_effect_texture_pack(pack({descriptor(62)}, 4, 20), 0, high);
  load_effect_texture_pack(pack({descriptor(254)}, 4, 220, 101), 0, high);
  require(high.palette[254 * 16][0] == 220 && high.textures[1].bank == 62 &&
              resolve_effect_texture(high, 1).rgba[0] == 20,
          "CPU staging bank and packed draw bank must stay distinct");
}
void aliases() {
  NativeEffectTextureLibrary wrap;
  for (size_t i = 0; i < 32; ++i) {
    const auto loaded = load_effect_texture_pack(pack({0xff00}), 0, wrap);
    require(loaded.first == i && loaded.count == 1 &&
                wrap.textures.back().bank == 32 + i,
            "Original bank sequence must cover32 through63");
  }
  require(wrap.next_bank == 32, "Automatic bank must wrap64 to32");
  const auto previous = wrap.textures[0].image;
  load_effect_texture_pack(pack({0xff00}, 4, 222, 100, {0xff, 0xff}), 0, wrap);
  require(wrap.next_bank == 33 && wrap.textures.back().image == previous &&
              resolve_effect_texture(wrap, 32).rgba[0] == 222 &&
              resolve_effect_texture(wrap, 0).rgba[0] == 222,
          "Primary identity reuse must retain old pixels and current colors");
  load_effect_texture_pack(pack({descriptor(32)}, 4, 180, 101, {0xff, 0xff}), 0,
                           wrap);
  require(wrap.textures.back().image != previous &&
              resolve_effect_texture(wrap, 33).rgba[0] == 203 &&
              resolve_effect_texture(wrap, 0).rgba[0] == 180,
          "Different primary identity keeps new indices but shares palette");

  NativeEffectTextureLibrary previous_entry;
  load_effect_texture_pack(pack({descriptor(32)}), 0, previous_entry);
  const auto original_image = previous_entry.textures[0].image;
  load_effect_texture_pack(pack({descriptor(32, 1), descriptor(33)}, 5, 80), 0,
                           previous_entry);
  require(previous_entry.textures.size() == 2 &&
              previous_entry.textures[1].image == original_image &&
              previous_entry.textures[1].image->type == 4 &&
              resolve_effect_texture(previous_entry, 1).rgba[0] == 120,
          "Nonfirst emitted descriptor must alias preceding entry's image");
  NativeEffectTextureLibrary no_parent;
  reject(
      [&] { load_effect_texture_pack(pack({0xff01, 0xff00}), 0, no_parent); },
      "A first alias without a preceding texture must reject");
  require(no_parent.textures.empty() && no_parent.next_bank == 32 &&
              no_parent.initialized.none(),
          "Failed alias must roll back palette writes and cursor");
  NativeEffectTextureLibrary masked;
  load_effect_texture_pack(pack({descriptor(32)}, 4, 10, 0xffffffff), 0,
                           masked);
  load_effect_texture_pack(
      pack({descriptor(32)}, 4, 30, 0x03ffffff, {0xff, 0xff}), 0, masked);
  require(masked.textures[1].image == masked.textures[0].image &&
              resolve_effect_texture(masked, 1).rgba[0] == 30,
          "Source primary global-index identity uses only26 bits");
}
void validation() {
  NativeEffectTextureLibrary library;
  load_effect_texture_pack(pack({0xff00}), 0, library);
  const auto before = library;
  auto malformed = pack({descriptor(32, 1)}, 4, 200);
  malformed.resize(malformed.size() - 4);
  record(malformed, 0x00494c50, Bytes{28, 0, 0, 0});
  reject([&] { load_effect_texture_pack(malformed, 0, library); },
         "Malformed second record must reject");
  require(library.next_bank == before.next_bank &&
              library.initialized == before.initialized &&
              library.palette == before.palette &&
              library.emitted_pixels == before.emitted_pixels &&
              library.textures.size() == before.textures.size() &&
              library.textures[0].image == before.textures[0].image,
          "Failed pack must leave the incoming library unchanged");
  for (const auto &bad :
       {pack({descriptor(254)}, 5), pack({descriptor(254, 1, 32)}),
        pack({descriptor(0, 1, 65535)})})
    reject([&] { load_effect_texture_pack(bad, 0, library); },
           "Palette storage write overflow must reject");
  NativeEffectTextureLibrary undefined;
  load_effect_texture_pack(pack({descriptor(32, 0, 100)}), 0, undefined);
  reject([&] { resolve_effect_texture(undefined, 0); },
         "Unloaded palette colors must not become invented textures");
  reject([&] { resolve_effect_texture(library, 1); },
         "Texture index beyond library must reject");
  NativeEffectTextureLibrary invalid_cursor;
  invalid_cursor.next_bank = 64;
  reject([&] { load_effect_texture_pack(pack({0xff01}), 0, invalid_cursor); },
         "Invalid automatic bank cursor must reject");
  NativeEffectTextureLibrary budget;
  budget.emitted_pixels = 64 * 1024 * 1024 - 4;
  reject([&] { load_effect_texture_pack(pack({0xff00, 0xff00}), 0, budget); },
         "Every aliased emission must count toward pixel budget");
  require(budget.textures.empty() && budget.next_bank == 32 &&
              budget.emitted_pixels == 64 * 1024 * 1024 - 4 &&
              budget.initialized.none(),
          "Budget rejection must be atomic");
  budget.emitted_pixels = 64 * 1024 * 1024;
  load_effect_texture_pack(pack({0xff01}), 0, budget);
  require(budget.textures.empty() && budget.next_bank == 33,
          "Palette-only updates consume no image pixel budget");
}
void identity() {
  Bytes ordinary;
  auto initial = picture(1, 4, 10, 100);
  initial[144] = 3; // Ordinary CSM1 indexed texture, no shared palette bank.
  half(initial, 148, 2); // Retained image differs from later4-pixel upload.
  record(ordinary, 0x324d4954, initial);
  const auto end = ordinary.size();
  ordinary.resize(end + 4);
  word(ordinary, end, 0xffffffff);
  NativeEffectTextureLibrary library;
  load_effect_texture_pack(ordinary, 0, library);
  const auto previous = library.textures[0].direct;
  const auto original = resolve_effect_texture(library, 0);
  require(previous && !library.textures[0].image && original.width == 2 &&
              original.rgba == Bytes({10, 70, 90, 255, 26, 70, 90, 255}),
          "Ordinary predecessor fixture");
  load_effect_texture_pack(
      pack({descriptor(34, 1), descriptor(33)}, 4, 80, 200), 0, library);
  require(library.textures[1].direct == previous &&
              resolve_effect_texture(library, 1).rgba == original.rgba,
          "Shared alias must retain ordinary image and RGBA");
  load_effect_texture_pack(pack({descriptor(33)}, 4, 180, 100, {0xff, 0xff}), 0,
                           library);
  require(library.textures[2].direct == previous &&
              !library.textures[2].image &&
              resolve_effect_texture(library, 2).rgba == original.rgba &&
              library.emitted_pixels == 6,
          "Primary must reuse ordinary-backed alias identity and dimensions");

  NativeEffectTextureLibrary no_false_bank;
  load_effect_texture_pack(ordinary, 0, no_false_bank);
  load_effect_texture_pack(pack({descriptor(0)}, 4, 180, 100, {0xff, 0xff}), 0,
                           no_false_bank);
  require(no_false_bank.textures[1].image &&
              !no_false_bank.textures[1].direct &&
              resolve_effect_texture(no_false_bank, 1).rgba[0] == 203 &&
              no_false_bank.emitted_pixels == 6,
          "Ordinary source identity has no shared bank, including bank0");
}
void reference() {
  NativeEffectTextureLibrary small;
  load_effect_texture_pack(pack({0xff01}, 4, 0), 0, small);
  for (size_t i = 0; i < 16; ++i)
    require(small.palette[512 + i][0] == effect_palette_reference::small[i],
            "Original ClutCopy capture mismatch");
  NativeEffectTextureLibrary large;
  load_effect_texture_pack(pack({0xff01}, 5, 0), 0, large);
  uint64_t hash = 1469598103934665603ull;
  for (size_t i = 0; i < 256; ++i)
    for (size_t byte = 0; byte < 4; ++byte) {
      const auto index = uint32_t(large.palette[512 + i][0]);
      hash ^= (index >> (8 * byte)) & 255;
      hash *= 1099511628211ull;
    }
  require(hash == effect_palette_reference::large_hash,
          "Original ClutCopy256 capture mismatch");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Missing mode");
    const std::string mode = argv[1];
    if (mode == "palettes")
      palettes();
    else if (mode == "aliases")
      aliases();
    else if (mode == "validation")
      validation();
    else if (mode == "reference")
      reference();
    else if (mode == "identity")
      identity();
    else
      throw std::runtime_error("Unknown mode");
    std::cout << "Native effect palette bank " << mode << " passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
