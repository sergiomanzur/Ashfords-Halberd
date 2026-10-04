#include "effect_alpha_reference_fixture.hpp"
#include "scene.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace ashford;
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void reject(F action) {
  bool failed = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  require(failed, "Malformed/unsupported palette input was accepted");
}
void word(Bytes &b, size_t p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(value >> (8 * i));
}
void half(Bytes &b, size_t p, uint16_t value) {
  b.at(p) = uint8_t(value);
  b.at(p + 1) = uint8_t(value >> 8);
}
void record(Bytes &pack, uint32_t magic, const Bytes &payload) {
  const auto p = pack.size();
  pack.resize(p + 32);
  word(pack, p, magic);
  word(pack, p + 4, uint32_t(payload.size()));
  pack.insert(pack.end(), payload.begin(), payload.end());
}
void terminal(Bytes &pack) {
  const auto p = pack.size();
  pack.resize(p + 4);
  word(pack, p, 0xffffffff);
}
Bytes metadata(size_t palettes) {
  Bytes result(4 + palettes * 8);
  word(result, 0, uint32_t(palettes));
  for (size_t i = 0; i < palettes; ++i) {
    word(result, 4 + i * 8, uint32_t(900 + i)); // Original ignores this word.
    word(result, 8 + i * 8, 0x0000ff00);        // Independent automatic bank.
  }
  return result;
}
Bytes picture(size_t palettes = 2, uint8_t original = 2) {
  Bytes result(256 + 2 + palettes * 128);
  result[0] = 'T';
  result[1] = 'I';
  result[2] = 'M';
  result[3] = '2';
  result[4] = 4;
  result[5] = 1;
  half(result, 6, 1);
  result[12] = original;
  word(result, 128, uint32_t(result.size() - 128));
  word(result, 132, uint32_t(palettes * 128));
  word(result, 136, 2);
  half(result, 140, 128);
  half(result, 142, 32);
  result[144] = 5;
  result[146] = 3;
  result[147] = 4;
  half(result, 148, 4);
  half(result, 150, 1);
  result[256] = 0xf8; // Low nibble first; ClutCopy maps8/15 to16/23.
  result[257] = 0x01;
  for (size_t palette = 0; palette < palettes; ++palette)
    for (size_t index = 0; index < 16; ++index) {
      const size_t p =
          258 + palette * 128 + (index < 8 ? index : index + 8) * 4;
      result[p] = uint8_t(17 + palette * 51 + index);
      result[p + 1] = 34;
      result[p + 2] = 51;
      result[p + 3] = 0xf0;
    }
  return result;
}
Bytes pack(size_t palettes = 2) {
  Bytes result;
  record(result, 0x00494c50, metadata(palettes));
  record(result, 0x324d4954, picture(palettes));
  terminal(result);
  return result;
}
void variants() {
  const auto source = pack();
  const auto decoded = decode_texture_pack(source, 0);
  require(decoded.size() == 2, "Shared image must emit two palette textures");
  for (const auto &t : decoded)
    require(t.width == 4 && t.height == 1 && t.rgba.size() == 16,
            "Native variant extent");
  require(decoded[0].rgba == Bytes({25, 34, 51, 255, 32, 34, 51, 255, 18, 34,
                                    51, 255, 17, 34, 51, 255}) &&
              decoded[1].rgba == Bytes({76, 34, 51, 255, 83, 34, 51, 255, 69,
                                        34, 51, 255, 68, 34, 51, 255}),
          "Variant order, nibble order and padded source CLUT addressing");
  require(source == pack(), "Decoding must not mutate imported bytes");
  auto single_image = picture(1);
  half(single_image, 142,
       16); // Logical colors exclude the source CLUT padding.
  Bytes single_pack;
  record(single_pack, 0x00494c50, metadata(1));
  record(single_pack, 0x324d4954, single_image);
  terminal(single_pack);
  const auto single = decode_texture_pack(single_pack, 0);
  require(single.size() == 1 && single[0].rgba == decoded[0].rgba,
          "Logical16-color header must retain padded CLUT addressing");
  Bytes retained;
  record(retained, 0x00494c50, metadata(2));
  record(retained, 0x324d4954, picture());
  record(retained, 0x00494c50, {}); // Source empty PLI retains previous values.
  record(retained, 0x324d4954, picture());
  record(retained, 0x00494c50, metadata(1));
  record(retained, 0x324d4954, picture(1));
  terminal(retained);
  const auto all = decode_texture_pack(retained, 0);
  require(all.size() == 5 && all[0].rgba == all[2].rgba &&
              all[1].rgba == all[3].rgba && all[4].rgba == all[0].rgba,
          "Persistent PLI metadata across images and replacement");
  // Shared8-bit palettes use the same8/16 swap in every32-color group.
  auto eight = picture(2, 6);
  eight.resize(256 + 5 + 2048);
  word(eight, 128, uint32_t(eight.size() - 128));
  word(eight, 132, 2048);
  word(eight, 136, 5);
  half(eight, 142, 256);
  eight[147] = 5;
  half(eight, 148, 5);
  for (size_t i = 0; i < 5; ++i)
    eight[256 + i] = std::array<uint8_t, 5>{0, 8, 16, 24, 255}[i];
  for (size_t palette = 0; palette < 2; ++palette)
    for (size_t index = 0; index < 256; ++index) {
      const size_t p = 261 + palette * 1024 + index * 4;
      eight[p] = uint8_t(17 + palette * 51 + index);
      eight[p + 1] = 34;
      eight[p + 2] = 51;
      eight[p + 3] = 128;
    }
  Bytes eight_pack;
  record(eight_pack, 0x00494c50, metadata(2));
  record(eight_pack, 0x324d4954, eight);
  terminal(eight_pack);
  const auto eight_textures = decode_texture_pack(eight_pack, 0);
  require(eight_textures.size() == 2 &&
              eight_textures[0].rgba ==
                  Bytes({17, 34,  51, 128, 33, 34,  51, 128, 25, 34,
                         51, 128, 41, 34,  51, 128, 16, 34,  51, 128}) &&
              eight_textures[1].rgba ==
                  Bytes({68, 34,  51, 128, 84, 34,  51, 128, 76, 34,
                         51, 128, 92, 34,  51, 128, 67, 34,  51, 128}),
          "Shared8-bit palette strides and source CLUT permutation");
}
void reference() {
  for (const auto &c : effect_alpha_reference::cases) {
    auto image = picture(1, c.original);
    image[256] = 0;
    const size_t p = 258;
    image[p] = c.black ? 0 : 17;
    image[p + 1] = c.black ? 0 : 34;
    image[p + 2] = c.black ? 0 : 51;
    image[p + 3] = c.alpha;
    Bytes source;
    record(source, 0x00494c50, metadata(1));
    record(source, 0x324d4954, image);
    terminal(source);
    const auto decoded = decode_texture_pack(source, 0);
    require(decoded.size() == 1 && decoded[0].rgba[0] == image[p] &&
                decoded[0].rgba[1] == image[p + 1] &&
                decoded[0].rgba[2] == image[p + 2] &&
                decoded[0].rgba[3] ==
                    uint8_t(std::min(255u, unsigned(c.gs_alpha) * 2)),
            "Original alpha helper capture mismatch");
  }
}
void validation() {
  const auto stable = pack();
  for (size_t length : {size_t(0), size_t(3), size_t(31), size_t(35),
                        size_t(51), stable.size() - 1}) {
    auto truncated = stable;
    truncated.resize(length);
    reject([&] { decode_texture_pack(truncated, 0); });
  }
  reject(
      [&] { decode_texture_pack(stable, std::numeric_limits<size_t>::max()); });
  for (uint32_t count : {0u, 28u, 0xffffffffu}) {
    auto invalid = stable;
    word(invalid, 32, count);
    reject([&] { decode_texture_pack(invalid, 0); });
  }
  for (uint32_t descriptor : {0x0000ff01u, 0x00000000u, 0x0001ff00u}) {
    auto invalid = stable;
    word(invalid, 40, descriptor);
    reject([&] { decode_texture_pack(invalid, 0); });
  }
  auto invalid = stable;
  word(invalid, 4, 12); // Count2 with only one metadata entry.
  reject([&] { decode_texture_pack(invalid, 0); });
  auto test_image = [&](Bytes image) {
    Bytes bytes;
    record(bytes, 0x00494c50, metadata(2));
    record(bytes, 0x324d4954, image);
    terminal(bytes);
    reject([&] { decode_texture_pack(bytes, 0); });
  };
  test_image(picture(1)); // Missing second palette.
  auto image = picture();
  image[144] = 6;
  test_image(image);
  image = picture();
  image[12] = 3;
  test_image(image);
  image = picture();
  half(image, 14,
       0x8000); // Prepared alpha/pixel conversion has a separate layout.
  test_image(image);
  image = picture();
  image[146] = 0x83;
  test_image(image);
  image = picture();
  half(image, 148, 4096);
  half(image, 150, 4096);
  test_image(image);
  image = picture();
  half(image, 140, 48);
  test_image(image);
  image = picture();
  word(image, 128, 0xffffffff);
  test_image(image);
  image = picture();
  half(image, 142, 65535);
  test_image(image);
  Bytes unbound;
  record(unbound, 0x324d4954, picture());
  terminal(unbound);
  reject([&] { decode_texture_pack(unbound, 0); });
  // Valid source extents, but five16M-pixel variants exceed the64M pack budget.
  // The rejection must precede allocating their320MiB of output pixels.
  auto over_budget = picture(5);
  constexpr size_t image_bytes = 4096 * 4096 / 2;
  over_budget.resize(256 + image_bytes + 5 * 128);
  word(over_budget, 128, uint32_t(over_budget.size() - 128));
  word(over_budget, 132, 5 * 128);
  word(over_budget, 136, uint32_t(image_bytes));
  half(over_budget, 148, 4096);
  half(over_budget, 150, 4096);
  Bytes oversized;
  record(oversized, 0x00494c50, metadata(5));
  record(oversized, 0x324d4954, over_budget);
  terminal(oversized);
  bool budget_rejected = false;
  try {
    decode_texture_pack(oversized, 0);
  } catch (const std::runtime_error &e) {
    budget_rejected = std::string(e.what()) == "Texture pack limit exceeded";
  }
  require(budget_rejected,
          "Palette variants must count toward aggregate pixels");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Missing test mode");
    const std::string mode = argv[1];
    if (mode == "variants")
      variants();
    else if (mode == "reference")
      reference();
    else if (mode == "validation")
      validation();
    else
      throw std::runtime_error("Unknown test mode");
    std::cout << "Native effect texture " << mode << " passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
