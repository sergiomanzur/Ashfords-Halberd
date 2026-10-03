#include "scene.hpp"
// File layouts and chunk behavior follow the MIT-licensed recvx-decomp:
// binfunc.c, ps2_NinjaCnk.c, ps2_NaMatrix.c, ps2_texture.c and types.h.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <stdexcept>
namespace ashford {
namespace {
struct Reader {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Scene range exceeds input");
  }
  uint16_t u16(size_t p) const {
    range(p, 2);
    return uint16_t(b[p]) | uint16_t(b[p + 1]) << 8;
  }
  uint32_t u32(size_t p) const {
    range(p, 4);
    return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16;
  }
  float f32(size_t p) const {
    auto bits = u32(p);
    float f;
    std::memcpy(&f, &bits, 4);
    if (!std::isfinite(f))
      throw std::runtime_error("Non-finite scene value");
    return f;
  }
  Vec3 vec(size_t p) const { return {f32(p), f32(p + 4), f32(p + 8)}; }
};
using Matrix = std::array<double, 16>;
Matrix identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }
Matrix multiply(const Matrix &a, const Matrix &b) {
  Matrix r{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      for (int k = 0; k < 4; ++k)
        r[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
  return r;
}
Vec3 transform(const Matrix &m, Vec3 p) {
  Vec3 r{float(m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3]),
         float(m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7]),
         float(m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11])};
  if (!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.z))
    throw std::runtime_error("Transform overflow");
  return r;
}
Matrix rotation(int axis, uint32_t angle) {
  auto r = identity();
  double a = double(angle & 65535) * 6.283185307179586 / 65536.;
  double c = std::cos(a), s = std::sin(a);
  int i = (axis + 1) % 3, j = (axis + 2) % 3;
  r[i * 4 + i] = c;
  r[j * 4 + j] = c;
  r[i * 4 + j] = -s;
  r[j * 4 + i] = s;
  return r;
}
Mesh chunks(const Reader &r, size_t base, size_t model, const Matrix &world,
            uint32_t object) {
  r.range(model, 24);
  Mesh mesh;
  mesh.object = object;
  const auto vo = r.u32(model), po = r.u32(model + 4);
  if (vo == 0xffffffff || po == 0xffffffff)
    return mesh;
  size_t p = base + vo;
  std::vector<Vec3> vertices(65536);
  std::vector<bool> valid(65536);
  bool ended = false;
  for (size_t n = 0; n < 4096; ++n) {
    r.range(p, 2);
    auto type = r.b[p];
    if (type == 255) {
      ended = true;
      break;
    }
    if (type != 51)
      throw std::runtime_error("Unsupported Ninja vertex chunk " +
                               std::to_string(type));
    r.range(p, 64);
    auto start = r.u16(p + 4), count = r.u16(p + 6);
    if (size_t(start) + count > vertices.size())
      throw std::runtime_error("Vertex cache overflow");
    auto bytes = size_t(count) * 32 + 64;
    r.range(p, bytes);
    if (4 + size_t(r.u16(p + 2)) * 4 != bytes)
      throw std::runtime_error("Invalid PS2 vertex chunk length");
    for (size_t i = 0; i < count; ++i) {
      vertices[start + i] = transform(world, r.vec(p + 64 + i * 32));
      valid[start + i] = true;
    }
    p += bytes;
  }
  if (!ended)
    throw std::runtime_error("Unterminated vertex list");
  p = base + po;
  ended = false;
  uint16_t texture = 0;
  std::array<uint8_t, 4> color{255, 255, 255, 255};
  for (size_t n = 0; n < 4096; ++n) {
    r.range(p, 2);
    auto type = r.b[p];
    if (type == 255) {
      ended = true;
      break;
    }
    if (type < 8) {
      p += 2;
      continue;
    }
    if (type < 16) {
      r.range(p, 4);
      if (type == 8)
        texture = r.u16(p + 2) & 0xfff;
      p += 4;
      continue;
    }
    r.range(p, 4);
    size_t end = p + 4 + size_t(r.u16(p + 2)) * 2;
    r.range(p, end - p);
    if (type >= 16 && type <= 23) {
      if (type == 17 || type == 19 || type == 21 || type == 23) {
        if (end - p < 8)
          throw std::runtime_error("Truncated diffuse material");
        color = {r.b[p + 6], r.b[p + 5], r.b[p + 4], r.b[p + 7]};
      }
      p = end;
      continue;
    }
    if (type != 64 && type != 65 && type != 66)
      throw std::runtime_error("Unsupported Ninja polygon chunk " +
                               std::to_string(type));
    size_t cursor = p + 4;
    auto take = [&]() {
      if (cursor + 2 > end)
        throw std::runtime_error("Truncated triangle strip");
      auto v = r.u16(cursor);
      cursor += 2;
      return v;
    };
    const auto strips = take();
    const auto extra = strips >> 14;
    if (type != 64 && extra)
      throw std::runtime_error("Unsupported UV strip user data");
    for (size_t strip = 0; strip < (strips & 0x3fff); ++strip) {
      auto signed_count = int16_t(take());
      int count = std::abs(int(signed_count));
      if (count < 3)
        throw std::runtime_error("Invalid triangle strip count");
      std::vector<Corner> corners;
      corners.reserve(size_t(count));
      for (int i = 0; i < count; ++i) {
        auto index = take();
        if (!valid[index])
          throw std::runtime_error("Triangle references missing vertex");
        Corner c;
        c.position = vertices[index];
        if (type != 64) {
          c.u = float(take()) / (type == 65 ? 255.f : 1023.f);
          c.v = float(take()) / (type == 65 ? 255.f : 1023.f);
        }
        corners.push_back(c);
        if (type == 64 && i >= 2)
          for (int k = 0; k < extra; ++k)
            take();
      }
      for (int i = 2; i < count; ++i) {
        Triangle t;
        t.texture = type == 64 ? 0xffff : texture;
        t.color = color;
        t.corners = {corners[i - 2], corners[i - 1], corners[i]};
        if (((i & 1) != 0) ^ (signed_count < 0))
          std::swap(t.corners[0], t.corners[1]);
        mesh.triangles.push_back(t);
        if (mesh.triangles.size() > 500000)
          throw std::runtime_error("Triangle limit exceeded");
      }
    }
    // Retail strip records may include one zero short for 32-bit alignment.
    if (cursor + 2 == end && cursor % 4 == 2 && r.u16(cursor) == 0)
      cursor += 2;
    if (cursor != end)
      throw std::runtime_error("Strip length mismatch");
    p = end;
  }
  if (!ended)
    throw std::runtime_error("Unterminated polygon list");
  return mesh;
}
} // namespace
std::vector<Mesh> decode_model(const Bytes &data, size_t offset,bool include_hidden,
                              const std::vector<ModelMatrix>* matrices) {
  Reader r{data};
  r.range(offset, 16);
  if (data[offset] != 'M' || data[offset + 1] != 'D' ||
      data[offset + 2] != 'L' || !(data[offset + 3] & 128))
    throw std::runtime_error("Unsupported model encoding");
  auto header = r.u16(offset + 4), count = r.u16(offset + 6);
  if (header < 16 || count > 4096)
    throw std::runtime_error("Invalid model header");
  size_t base = offset + header, table = base + r.u32(offset + 12);
  r.range(offset, header);
  r.range(table, size_t(count) * 52);
  if(matrices && matrices->size()!=count)throw std::runtime_error("Invalid native model matrices");
  if (!count)
    return {};
  std::vector<Mesh> meshes;
  std::vector<bool> active(count), visited(count);
  size_t total = 0;
  auto node = [&](uint32_t off) {
    size_t p = base + off;
    if (p < table || (p - table) % 52 || (p - table) / 52 >= count)
      throw std::runtime_error("Invalid object link");
    return (p - table) / 52;
  };
  std::function<void(size_t, const Matrix &, size_t)> visit =
      [&](size_t i, const Matrix &parent, size_t depth) {
        if (depth > 256 || active[i] || visited[i])
          throw std::runtime_error("Cyclic or shared object hierarchy");
        active[i] = true;
        visited[i] = true;
        size_t o = table + i * 52;
        auto flags = r.u32(o);
        auto local = identity();
        if (!(flags & 1)) {
          auto pos = r.vec(o + 8);
          local[3] = pos.x;
          local[7] = pos.y;
          local[11] = pos.z;
        }
        if (!(flags & 2)) {
          std::array<int, 3> order = (flags & 32) ? std::array<int, 3>{1, 0, 2}
                                                  : std::array<int, 3>{2, 1, 0};
          for (int axis : order)
            local = multiply(local, rotation(axis, r.u32(o + 20 + axis * 4)));
        }
        if (!(flags & 4)) {
          auto s = r.vec(o + 32);
          auto scale = identity();
          scale[0] = s.x;
          scale[5] = s.y;
          scale[10] = s.z;
          local = multiply(local, scale);
        }
        auto world = multiply(parent, local);
        auto model = r.u32(o + 4);
        if ((include_hidden || !(flags & 8)) && model != 0xffffffff) {
          auto mesh = chunks(r, base, base + model, matrices?matrices->at(i):world, uint32_t(i));
          mesh.initially_hidden=bool(flags&8);
          total += mesh.triangles.size();
          if (total > 2000000)
            throw std::runtime_error("Scene triangle limit exceeded");
          meshes.push_back(std::move(mesh));
        }
        auto child = r.u32(o + 44), sibling = r.u32(o + 48);
        if ((matrices || !(flags & 16)) && child != 0xffffffff)
          visit(node(child), world, depth + 1);
        active[i] = false;
        if (sibling != 0xffffffff)
          visit(node(sibling), parent, depth + 1);
      };
  visit(0, identity(), 0);
  return meshes;
}
std::vector<Mesh> decode_model_range(const Bytes &data, size_t offset,
                                    const std::vector<ModelMatrix> &matrices,
                                    size_t first_object, size_t object_count) {
  Reader r{data};
  r.range(offset, 16);
  if (data[offset] != 'M' || data[offset + 1] != 'D' ||
      data[offset + 2] != 'L' || !(data[offset + 3] & 128))
    throw std::runtime_error("Unsupported model encoding");
  const size_t header = r.u16(offset + 4), count = r.u16(offset + 6);
  if (header < 16 || count > 4096 || matrices.size() != count ||
      first_object > count || object_count > count - first_object)
    throw std::runtime_error("Invalid native model drawing range");
  const size_t base = offset + header, table = base + r.u32(offset + 12);
  r.range(offset, header);
  r.range(table, count * 52);
  std::vector<Mesh> meshes;
  size_t total = 0;
  for (size_t i = first_object; i < first_object + object_count; ++i) {
    const auto &matrix = matrices[i];
    for (double value : matrix)
      if (!std::isfinite(value))
        throw std::runtime_error("Non-finite native model drawing matrix");
    if (matrix[12] != 0 || matrix[13] != 0 || matrix[14] != 0 || matrix[15] != 1)
      throw std::runtime_error("Native model drawing matrix is not affine");
    const size_t o = table + i * 52;
    const auto flags = r.u32(o), model = r.u32(o + 4);
    if (!(flags & 8) && model != 0xffffffff) {
      auto mesh = chunks(r, base, base + model, matrix, uint32_t(i));
      total += mesh.triangles.size();
      if (total > 2000000)
        throw std::runtime_error("Scene triangle limit exceeded");
      meshes.push_back(std::move(mesh));
    }
  }
  return meshes;
}
Texture decode_tim2(const Bytes &data, size_t offset, size_t size) {
  Reader r{data};
  r.range(offset, size);
  if (size < 16 || data[offset] != 'T' || data[offset + 1] != 'I' ||
      data[offset + 2] != 'M' || data[offset + 3] != '2')
    throw std::runtime_error("Invalid TIM2 signature");
  if (data[offset + 4] != 4 || data[offset + 5] > 1 || r.u16(offset + 6) != 1)
    throw std::runtime_error("Unsupported TIM2 layout");
  size_t pic = offset + (data[offset + 5] ? 128 : 16);
  r.range(pic, 48);
  if (pic - offset + 48 > size)
    throw std::runtime_error("Truncated TIM2 header");
  auto total = r.u32(pic), clut = r.u32(pic + 4), image = r.u32(pic + 8);
  auto header = r.u16(pic + 12), colors = r.u16(pic + 14);
  auto type = data[pic + 19], clut_type = data[pic + 18];
  Texture t;
  t.width = r.u16(pic + 20);
  t.height = r.u16(pic + 22);
  if (!t.width || !t.height || t.width > 4096 || t.height > 4096 ||
      header < 48 || uint64_t(header) + image + clut > total ||
      total > size - (pic - offset))
    throw std::runtime_error("Invalid TIM2 sizes");
  r.range(pic, total);
  size_t pixels = size_t(t.width) * t.height, source = pic + header,
         palette = source + image;
  if (type != 3 && type != 4 && type != 5)
    throw std::runtime_error("Unsupported TIM2 image type");
  auto required = type == 3   ? pixels * 4
                  : type == 5 ? pixels
                              : (pixels + 1) / 2;
  if (image < required)
    throw std::runtime_error("Truncated TIM2 image");
  if (type != 3 &&
      ((clut_type & 0x7f) != 3 || colors < (type == 5 ? 256 : 16) ||
       clut < size_t(colors) * 4))
    throw std::runtime_error("Unsupported TIM2 palette");
  // Supported retail packs contain linear indexed pixels. Palette changes,
  // mip selection, and GPU transfer packets are not decoded as image pixels.
  // ps2_texture.c::isVQ distinguishes prepared transfer data (5..8) from
  // ordinary indexed image storage. Inspected room atlases use 1, 3 or 16.
  auto format = data[pic + 16];
  if (format > 4 && format != 16)
    throw std::runtime_error("Unsupported TIM2 transfer format " +
                             std::to_string(format));
  t.rgba.resize(pixels * 4);
  for (size_t i = 0; i < pixels; ++i) {
    size_t p = source + i * 4;
    if (type != 3) {
      unsigned index = type == 5 ? data[source + i]
                                 : (data[source + i / 2] >> (i % 2 * 4)) & 15;
      if (!(clut_type & 0x80))
        index = (index & ~24u) | ((index & 8u) << 1) | ((index & 16u) >> 1);
      if (size_t(index) * 4 + 4 > clut)
        throw std::runtime_error("TIM2 palette index exceeds stored colors");
      p = palette + index * 4;
    }
    for (int j = 0; j < 3; ++j)
      t.rgba[i * 4 + j] = data[p + j];
    t.rgba[i * 4 + 3] = uint8_t(std::min(255u, unsigned(data[p + 3]) * 2));
  }
  return t;
}
std::vector<Texture> decode_texture_pack(const Bytes &data, size_t offset) {
  Reader r{data};
  std::vector<Texture> textures;
  size_t p = offset, total_pixels = 0;
  for (size_t n = 0; n < 4096; ++n) {
    auto magic = r.u32(p);
    if (magic == 0xffffffff)
      return textures;
    r.range(p,32);
    auto size = r.u32(p + 4);
    r.range(p + 32, size);
    if (magic == 0x324d4954) {
      auto t = decode_tim2(data, p + 32, size);
      total_pixels += t.rgba.size() / 4;
      if (total_pixels > 64 * 1024 * 1024)
        throw std::runtime_error("Texture pack limit exceeded");
      textures.push_back(std::move(t));
    } else if (magic != 0x00494c50 || size != 0)
      throw std::runtime_error("Unsupported texture pack record");
    p += 32 + size;
  }
  throw std::runtime_error("Unterminated texture pack");
}
Scene decode_room_scene(const Bytes &data) {
  room_info(data);
  Reader r{data};
  auto model_table = r.u32(20), tex_table = r.u32(32);
  if (!model_table || !tex_table || !r.u32(tex_table))
    throw std::runtime_error("Missing room scene tables");
  Scene s;
  s.meshes = decode_model(data, r.u32(model_table),true);
  s.textures = decode_texture_pack(data, r.u32(tex_table + 4));
  for (const auto &m : s.meshes)
    for (const auto &t : m.triangles)
      if (t.texture != 0xffff && t.texture >= s.textures.size())
        throw std::runtime_error("Missing triangle texture " +
                                 std::to_string(t.texture) + " of " +
                                 std::to_string(s.textures.size()));
  return s;
}
} // namespace ashford
