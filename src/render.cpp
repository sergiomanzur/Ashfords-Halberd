#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace ashford {
namespace {
struct Projected {
  double x, y, z;
};
Projected project(Vec3 v) {
  return {.70710678 * (v.x - v.z), -.40824829 * (v.x + v.z) + .81649658 * v.y,
          -.57735027 * (v.x + v.y + v.z)};
}
double edge(Projected a, Projected b, double x, double y) {
  return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
} // namespace
RasterImage render_overview(const Scene &scene, uint32_t width,
                            uint32_t height) {
  if (width < 1 || height < 1 || width > 2048 || height > 2048)
    throw std::runtime_error("Invalid raster dimensions");
  RasterImage image{width, height, Bytes(size_t(width) * height * 3)};
  for (size_t i = 0; i < image.rgb.size(); i += 3) {
    image.rgb[i] = 18;
    image.rgb[i + 1] = 22;
    image.rgb[i + 2] = 28;
  }
  for (const auto &t : scene.textures)
    if (!t.width || !t.height ||
        t.rgba.size() != uint64_t(t.width) * t.height * 4)
      throw std::runtime_error("Invalid raster texture");
  double minx = std::numeric_limits<double>::infinity(), miny = minx,
         maxx = -minx, maxy = -minx;
  size_t triangles = 0;
  for (const auto &m : scene.meshes)
    for (const auto &t : m.triangles) {
      if (++triangles > 2000000 ||
          (t.texture != 0xffff && t.texture >= scene.textures.size()))
        throw std::runtime_error("Invalid raster scene");
      for (auto c : t.corners) {
        auto p = project(c.position);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
            !std::isfinite(c.u) || !std::isfinite(c.v))
          throw std::runtime_error("Non-finite raster vertex");
        minx = std::min(minx, p.x);
        miny = std::min(miny, p.y);
        maxx = std::max(maxx, p.x);
        maxy = std::max(maxy, p.y);
      }
    }
  if (!triangles)
    return image;
  double scale = .9 * std::min(width / std::max(1e-6, maxx - minx),
                               height / std::max(1e-6, maxy - miny));
  std::vector<double> depth(size_t(width) * height,
                            std::numeric_limits<double>::infinity());
  for (const auto &m : scene.meshes)
    for (const auto &t : m.triangles) {
      std::array<Projected, 3> p;
      for (int i = 0; i < 3; ++i) {
        p[i] = project(t.corners[i].position);
        p[i].x = (p[i].x - (minx + maxx) * .5) * scale + width * .5;
        p[i].y = height * .5 - (p[i].y - (miny + maxy) * .5) * scale;
      }
      const auto area = edge(p[0], p[1], p[2].x, p[2].y);
      if (std::abs(area) < 1e-8)
        continue;
      int x0 = std::max(0, int(std::floor(std::min({p[0].x, p[1].x, p[2].x})))),
          x1 = std::min(int(width) - 1,
                        int(std::ceil(std::max({p[0].x, p[1].x, p[2].x}))));
      int y0 = std::max(0, int(std::floor(std::min({p[0].y, p[1].y, p[2].y})))),
          y1 = std::min(int(height) - 1,
                        int(std::ceil(std::max({p[0].y, p[1].y, p[2].y}))));
      auto a = t.corners[0].position, b = t.corners[1].position,
           c = t.corners[2].position;
      double ax = double(b.x) - a.x, ay = double(b.y) - a.y,
             az = double(b.z) - a.z, bx = double(c.x) - a.x,
             by = double(c.y) - a.y, bz = double(c.z) - a.z;
      double nx = ay * bz - az * by, ny = az * bx - ax * bz,
             nz = ax * by - ay * bx;
      double length = std::sqrt(nx * nx + ny * ny + nz * nz),
             light = length > .00001
                         ? .4 + .6 * std::abs((nx * .3 + ny * .8 + nz * .5) /
                                              (length * .98994949))
                         : .5;
      const Texture *tex =
          t.texture == 0xffff ? nullptr : &scene.textures[t.texture];
      for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
          double w0 = edge(p[1], p[2], x + .5, y + .5) / area,
                 w1 = edge(p[2], p[0], x + .5, y + .5) / area, w2 = 1 - w0 - w1;
          if (w0 < 0 || w1 < 0 || w2 < 0)
            continue;
          double z = w0 * p[0].z + w1 * p[1].z + w2 * p[2].z;
          size_t pixel = size_t(y) * width + x;
          if (z >= depth[pixel])
            continue;
          double u = w0 * t.corners[0].u + w1 * t.corners[1].u +
                     w2 * t.corners[2].u,
                 v = w0 * t.corners[0].v + w1 * t.corners[1].v +
                     w2 * t.corners[2].v;
          u -= std::floor(u);
          v -= std::floor(v);
          const uint8_t *color = t.color.data();
          if (tex) {
            auto tx = std::min(tex->width - 1, uint32_t(u * tex->width)),
                 ty = std::min(tex->height - 1, uint32_t(v * tex->height));
            size_t sample = (size_t(ty) * tex->width + tx) * 4;
            color = &tex->rgba[sample];
          }
          if (color[3] < 128)
            continue;
          depth[pixel] = z;
          for (int channel = 0; channel < 3; ++channel)
            image.rgb[pixel * 3 + channel] =
                uint8_t(std::min(255., color[channel] * light));
        }
    }
  return image;
}
} // namespace ashford
