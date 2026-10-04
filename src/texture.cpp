#include "scene.hpp"
#include <algorithm>
#include <stdexcept>
// Native file pixels and shared palettes from recvx-decomp ps2_texture.c,
// ps2_dummy.c::Ps2CheckTextureAlpha and ps2_pxlconv.c::Tim2_Format_Check.
// Files hold linear indices; source conversion prepares a PSMCT32 upload.
namespace ashford {
namespace {
constexpr size_t pixel_limit = 64 * 1024 * 1024;
struct Reader {
  const Bytes &data;
  void range(size_t p, size_t n) const {
    if (p > data.size() || n > data.size() - p)
      throw std::runtime_error("Texture extent exceeds input");
  }
  uint16_t u16(size_t p) const {
    range(p, 2);
    return uint16_t(data[p]) | uint16_t(data[p + 1]) << 8;
  }
  uint32_t u32(size_t p) const {
    range(p, 4);
    return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16;
  }
};
struct Picture {
  size_t file = 0, source = 0, palette = 0, pixels = 0, clut_bytes = 0;
  uint32_t width = 0, height = 0;
  uint16_t colors = 0, header = 0;
  uint8_t type = 0, format = 0, clut_type = 0;
};
Picture read_picture(const Bytes &data, size_t offset, size_t size) {
  Reader r{data};
  r.range(offset, size);
  if (size < 16 || r.u32(offset) != 0x324d4954)
    throw std::runtime_error("Invalid TIM2 signature");
  if (data[offset + 4] != 4 || data[offset + 5] > 1 || r.u16(offset + 6) != 1)
    throw std::runtime_error("Unsupported TIM2 layout");
  const size_t prefix = data[offset + 5] ? 128 : 16;
  if (size < prefix + 48)
    throw std::runtime_error("Truncated TIM2 header");
  const size_t pic = offset + prefix;
  const auto total = r.u32(pic), clut = r.u32(pic + 4), image = r.u32(pic + 8);
  Picture result;
  result.file = offset;
  result.header = r.u16(pic + 12);
  result.colors = r.u16(pic + 14);
  result.type = data[pic + 19];
  result.format = data[pic + 16];
  result.clut_type = data[pic + 18];
  result.width = r.u16(pic + 20);
  result.height = r.u16(pic + 22);
  if (!result.width || !result.height || result.width > 4096 ||
      result.height > 4096 || result.header < 48 ||
      uint64_t(result.header) + image + clut > total || total > size - prefix)
    throw std::runtime_error("Invalid TIM2 sizes");
  result.pixels = size_t(result.width) * result.height;
  result.source = pic + result.header;
  result.palette = result.source + image;
  result.clut_bytes = clut;
  if (result.type != 3 && result.type != 4 && result.type != 5)
    throw std::runtime_error("Unsupported TIM2 image type");
  const size_t required = result.type == 3   ? result.pixels * 4
                          : result.type == 5 ? result.pixels
                                             : (result.pixels + 1) / 2;
  if (image < required)
    throw std::runtime_error("Truncated TIM2 image");
  r.range(pic, total);
  return result;
}
void validate_palette(const Picture &p, bool shared, size_t variants) {
  if (p.type == 3) {
    if (shared)
      throw std::runtime_error("Shared palette requires indexed TIM2");
    return;
  }
  const size_t colors = p.type == 5 ? 256 : 16;
  const size_t stride = p.type == 4 ? 128 : 1024;
  const size_t required = shared ? variants * stride : size_t(p.colors) * 4;
  if ((p.clut_type & 0x7f) != 3 || p.colors < colors ||
      p.clut_bytes < required || p.clut_bytes < size_t(p.colors) * 4)
    throw std::runtime_error("Unsupported or truncated TIM2 palette");
}
Texture decode_picture(const Bytes &data, const Picture &p, bool shared,
                       size_t variant = 0) {
  Texture result{p.width, p.height, Bytes(p.pixels * 4)};
  const size_t palette = p.palette + variant * (p.type == 4 ? 128 : 1024);
  for (size_t i = 0; i < p.pixels; ++i) {
    size_t color = p.source + i * 4;
    if (p.type != 3) {
      unsigned index = p.type == 5
                           ? data[p.source + i]
                           : (data[p.source + i / 2] >> (i % 2 * 4)) & 15;
      // ClutCopy selects0..7,16..23 for indexed4; ClutCopy256 swaps8/16
      // in each32-color group. Ordinary CSM1 palettes use the same selection.
      if (shared || !(p.clut_type & 0x80))
        index = (index & ~24u) | ((index & 8u) << 1) | ((index & 16u) >> 1);
      if (!shared && size_t(index) * 4 + 4 > p.clut_bytes)
        throw std::runtime_error("TIM2 palette index exceeds stored colors");
      color = palette + index * 4;
    }
    for (size_t channel = 0; channel < 3; ++channel)
      result.rgba[i * 4 + channel] = data[color + channel];
    unsigned alpha = data[color + 3];
    if (shared) {
      const auto original = data[p.file + 12];
      if (alpha && !data[color] && !data[color + 1] && !data[color + 2])
        alpha = 0;
      if (original == 2)
        alpha |= alpha >> 4;
      if (original == 2 || original == 6)
        alpha = (alpha + 1) >> 1;
    }
    // Native RGBA stores full-range alpha; source drawing consumes GS0..128.
    result.rgba[i * 4 + 3] = uint8_t(std::min(255u, alpha * 2));
  }
  return result;
}
void validate_shared(const Bytes &data, const Picture &p,
                     const std::vector<uint32_t> &palettes) {
  Reader r{data};
  const auto original = data[p.file + 12];
  if (p.format != 5 || data[p.file + 5] != 1 || p.header != 128 ||
      p.clut_type != 3 || r.u16(p.file + 14) != 0 ||
      (original != 0 && original != 1 && original != 2 && original != 6))
    throw std::runtime_error("Unsupported shared TIM2 preparation");
  if (palettes.empty())
    throw std::runtime_error("Shared TIM2 requires its PLI metadata");
  for (auto descriptor : palettes)
    if (descriptor != 0x0000ff00)
      throw std::runtime_error("Shared palette updates or explicit banks "
                               "require their native service");
  validate_palette(p, true, palettes.size());
}
} // namespace
Texture decode_tim2(const Bytes &data, size_t offset, size_t size) {
  const auto p = read_picture(data, offset, size);
  if (p.format > 4 && p.format != 16)
    throw std::runtime_error("Unsupported TIM2 transfer format " +
                             std::to_string(p.format));
  validate_palette(p, false, 1);
  return decode_picture(data, p, false);
}
std::vector<Texture> decode_texture_pack(const Bytes &data, size_t offset) {
  Reader r{data};
  std::vector<Texture> textures;
  std::vector<uint32_t> palettes;
  size_t p = offset, total_pixels = 0;
  for (size_t records = 0; records < 4096; ++records) {
    const auto magic = r.u32(p);
    if (magic == 0xffffffff)
      return textures;
    r.range(p, 32);
    const size_t size = r.u32(p + 4), payload = p + 32;
    r.range(payload, size);
    if (magic == 0x00494c50) {
      if (size) {
        if (size < 4)
          throw std::runtime_error("Truncated PLI metadata");
        const size_t count = r.u32(payload);
        // TIM2_PICTUREHEADER_EX::PalData has27 words. Source writes every
        // descriptor there; wider metadata cannot represent a valid header.
        if (!count || count > 27 || size != 4 + count * 8)
          throw std::runtime_error("Invalid PLI palette count or extent");
        palettes.clear();
        for (size_t i = 0; i < count; ++i)
          palettes.push_back(r.u32(payload + 8 + i * 8));
      }
    } else if (magic == 0x324d4954) {
      const auto picture = read_picture(data, payload, size);
      const bool shared = picture.format >= 5 && picture.format < 9;
      size_t variants = 1;
      if (shared) {
        validate_shared(data, picture, palettes);
        variants = palettes.size();
      } else {
        if (picture.format > 4 && picture.format != 16)
          throw std::runtime_error("Unsupported TIM2 transfer format " +
                                   std::to_string(picture.format));
        validate_palette(picture, false, 1);
      }
      if (picture.pixels > (pixel_limit - total_pixels) / variants)
        throw std::runtime_error("Texture pack limit exceeded");
      total_pixels += picture.pixels * variants;
      for (size_t variant = 0; variant < variants; ++variant)
        textures.push_back(decode_picture(data, picture, shared, variant));
    } else
      throw std::runtime_error("Unsupported texture pack record");
    p = payload + size;
  }
  throw std::runtime_error("Unterminated texture pack");
}
} // namespace ashford
