#include "texture.hpp"
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
Texture decode_picture(const Bytes &data, const Picture &p) {
  Texture result{p.width, p.height, Bytes(p.pixels * 4)};
  for (size_t i = 0; i < p.pixels; ++i) {
    size_t color = p.source + i * 4;
    if (p.type != 3) {
      unsigned index = p.type == 5
                           ? data[p.source + i]
                           : (data[p.source + i / 2] >> (i % 2 * 4)) & 15;
      // ClutCopy selects0..7,16..23 for indexed4; ClutCopy256 swaps8/16
      // in each32-color group. Ordinary CSM1 palettes use the same selection.
      if (!(p.clut_type & 0x80))
        index = (index & ~24u) | ((index & 8u) << 1) | ((index & 16u) >> 1);
      if (size_t(index) * 4 + 4 > p.clut_bytes)
        throw std::runtime_error("TIM2 palette index exceeds stored colors");
      color = p.palette + index * 4;
    }
    for (size_t channel = 0; channel < 3; ++channel)
      result.rgba[i * 4 + channel] = data[color + channel];
    unsigned alpha = data[color + 3];
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
  validate_palette(p, true, palettes.size());
}
void charge_pixels(NativeEffectTextureLibrary &library, size_t pixels) {
  if (library.emitted_pixels > pixel_limit ||
      pixels > pixel_limit - library.emitted_pixels ||
      library.textures.size() >= 4096)
    throw std::runtime_error("Texture pack limit exceeded");
  library.emitted_pixels += pixels;
}
void load_shared(const Bytes &data, const Picture &p,
                 const std::vector<uint32_t> &palettes,
                 NativeEffectTextureLibrary &library) {
  validate_shared(data, p, palettes);
  const size_t colors = p.type == 4 ? 16 : 256;
  const size_t stride = p.type == 4 ? 128 : 1024;
  Reader r{data};
  for (size_t variant = 0; variant < palettes.size(); ++variant) {
    const auto descriptor = palettes[variant];
    unsigned bank = (descriptor >> 8) & 255;
    if (bank == 255) {
      if (library.next_bank >= 64)
        throw std::runtime_error("Invalid automatic palette cursor");
      bank = library.next_bank;
      library.next_bank += uint16_t(colors / 16);
      if (library.next_bank >= 64)
        library.next_bank -= 32;
    }
    const size_t first = bank * 16 + (descriptor >> 16);
    if (first > library.palette.size() ||
        colors > library.palette.size() - first)
      throw std::runtime_error("Palette write exceeds native memory");
    for (size_t index = 0; index < colors; ++index) {
      const size_t source_index =
          (index & ~size_t(24)) | ((index & 8) << 1) | ((index & 16) >> 1);
      const size_t source = p.palette + variant * stride + source_index * 4;
      auto &color = library.palette[first + index];
      std::copy_n(data.begin() + source, 4, color.begin());
      unsigned alpha = color[3];
      if (alpha && !color[0] && !color[1] && !color[2])
        alpha = 0;
      if (data[p.file + 12] == 2)
        alpha |= alpha >> 4;
      if (data[p.file + 12] == 2 || data[p.file + 12] == 6)
        alpha = (alpha + 1) >> 1;
      color[3] = uint8_t(alpha);
      library.initialized.set(first + index);
    }
    if (descriptor & 255)
      continue;
    NativeEffectTextureBinding binding;
    binding.bank = uint8_t(bank & 63); // Packed32-bit gidx retains only6 bits.
    binding.shared_bank = true;
    if (variant) {
      // njLoadTexture's0x41010000 alias copies the preceding texture entry,
      // not this TIM2's pixels. Palette-only entries do not advance that entry.
      if (library.textures.empty())
        throw std::runtime_error("Palette alias has no preceding texture");
      binding.image = library.textures.back().image;
      binding.direct = library.textures.back().direct;
      binding.global_index = library.textures.back().global_index;
    } else {
      const uint32_t global_index = r.u32(p.file + 8) & 0x3ffffff;
      binding.global_index = global_index;
      for (const auto &existing : library.textures)
        if (existing.shared_bank && existing.bank == binding.bank &&
            existing.global_index == global_index) {
          binding.image = existing.image;
          binding.direct = existing.direct;
          break;
        }
      if (!binding.image && !binding.direct) {
        // Check output budget before retaining pixels for a new binding.
        if (library.emitted_pixels > pixel_limit ||
            p.pixels > pixel_limit - library.emitted_pixels)
          throw std::runtime_error("Texture pack limit exceeded");
        auto image = std::make_shared<NativeIndexedTexture>();
        image->width = p.width;
        image->height = p.height;
        image->type = p.type;
        const size_t bytes = p.type == 4 ? (p.pixels + 1) / 2 : p.pixels;
        image->indices.assign(data.begin() + p.source,
                              data.begin() + p.source + bytes);
        binding.image = std::move(image);
      }
    }
    const size_t pixels =
        binding.image ? size_t(binding.image->width) * binding.image->height
                      : size_t(binding.direct->width) * binding.direct->height;
    charge_pixels(library, pixels);
    library.textures.push_back(std::move(binding));
  }
}
} // namespace
Texture decode_tim2(const Bytes &data, size_t offset, size_t size) {
  const auto p = read_picture(data, offset, size);
  if (p.format > 4 && p.format != 16)
    throw std::runtime_error("Unsupported TIM2 transfer format " +
                             std::to_string(p.format));
  validate_palette(p, false, 1);
  return decode_picture(data, p);
}
NativeEffectTextureRange
load_effect_texture_pack(const Bytes &data, size_t offset,
                         NativeEffectTextureLibrary &library) {
  Reader r{data};
  auto staged = library; // Shared immutable images make rollback inexpensive.
  const size_t first = staged.textures.size();
  std::vector<uint32_t> palettes;
  size_t p = offset;
  for (size_t records = 0; records < 4096; ++records) {
    const auto magic = r.u32(p);
    if (magic == 0xffffffff) {
      const auto count = staged.textures.size() - first;
      library = std::move(staged);
      return {first, count};
    }
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
      if (shared) {
        load_shared(data, picture, palettes, staged);
      } else {
        if (picture.format > 4 && picture.format != 16)
          throw std::runtime_error("Unsupported TIM2 transfer format " +
                                   std::to_string(picture.format));
        validate_palette(picture, false, 1);
        charge_pixels(staged, picture.pixels);
        NativeEffectTextureBinding binding;
        binding.global_index = r.u32(picture.file + 8);
        binding.direct =
            std::make_shared<Texture>(decode_picture(data, picture));
        staged.textures.push_back(std::move(binding));
      }
    } else
      throw std::runtime_error("Unsupported texture pack record");
    p = payload + size;
  }
  throw std::runtime_error("Unterminated texture pack");
}
Texture resolve_effect_texture(const NativeEffectTextureLibrary &library,
                               size_t texture) {
  if (texture >= library.textures.size())
    throw std::runtime_error("Effect texture index exceeds library");
  const auto &binding = library.textures[texture];
  if (binding.direct && !binding.image)
    return *binding.direct;
  if (!binding.image || binding.direct)
    throw std::runtime_error("Invalid native texture binding");
  const auto &image = *binding.image;
  const size_t pixels = size_t(image.width) * image.height;
  if (!image.width || !image.height || image.width > 4096 ||
      image.height > 4096 || (image.type != 4 && image.type != 5) ||
      binding.bank >= 64 ||
      image.indices.size() != (image.type == 4 ? (pixels + 1) / 2 : pixels))
    throw std::runtime_error("Invalid native indexed texture");
  const size_t first =
      size_t(image.type == 4 ? binding.bank : binding.bank & 0x30) * 16;
  Texture result{image.width, image.height, Bytes(pixels * 4)};
  for (size_t i = 0; i < pixels; ++i) {
    const auto index = image.type == 4
                           ? (image.indices[i / 2] >> (i % 2 * 4)) & 15
                           : image.indices[i];
    if (!library.initialized.test(first + index))
      throw std::runtime_error("Effect texture samples an unloaded palette");
    const auto &color = library.palette[first + index];
    std::copy_n(color.begin(), 3, result.rgba.begin() + i * 4);
    result.rgba[i * 4 + 3] = uint8_t(std::min(255u, unsigned(color[3]) * 2));
  }
  return result;
}
std::vector<Texture> decode_texture_pack(const Bytes &data, size_t offset) {
  NativeEffectTextureLibrary library;
  const auto range = load_effect_texture_pack(data, offset, library);
  std::vector<Texture> textures;
  textures.reserve(range.count);
  for (size_t i = range.first; i < range.first + range.count; ++i)
    textures.push_back(resolve_effect_texture(library, i));
  return textures;
}
} // namespace ashford
