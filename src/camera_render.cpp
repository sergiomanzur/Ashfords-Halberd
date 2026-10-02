#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace ashford {
namespace {
struct Vertex {
  double x, y, z, u, v;
};
double edge(const Vertex &a, const Vertex &b, double x, double y) {
  return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
} // namespace
RasterImage render_camera(const Scene &scene, const Camera &camera,
                          uint32_t width, uint32_t height) {
  if (!width || !height || width > 2048 || height > 2048 ||
      camera.perspective < 2 || camera.perspective >= 32768)
    throw std::runtime_error("Invalid camera raster parameters");
  for (const auto &tex : scene.textures)
    if (!tex.width || !tex.height ||
        tex.rgba.size() != uint64_t(tex.width) * tex.height * 4)
      throw std::runtime_error("Invalid camera texture");
  RasterImage image{width, height, Bytes(size_t(width) * height * 3)};
  for (size_t i = 0; i < image.rgb.size(); i += 3) {
    image.rgb[i] = 18;
    image.rgb[i + 1] = 22;
    image.rgb[i + 2] = 28;
  }
  std::vector<double> depth(size_t(width) * height,
                            std::numeric_limits<double>::infinity());
  // njSetPerspective uses integer division by two before converting to radians.
  double tan_half =
      std::tan(double(camera.perspective / 2) * 6.283185307179586 / 65536.);
  double fx = width * .5 / tan_half * 1.174, fy = width * .5 / tan_half;
  size_t total = 0;
  for (const auto &mesh : scene.meshes) {
    if (mesh.object < 512 &&
        (camera.hidden[mesh.object / 32] & (0x80000000u >> (mesh.object % 32))))
      continue;
    for (const auto &triangle : mesh.triangles) {
      if (++total > 2000000 || (triangle.texture != 0xffff &&
                                triangle.texture >= scene.textures.size()))
        throw std::runtime_error("Invalid camera scene");
      std::vector<Vertex> polygon;
      for (auto corner : triangle.corners) {
        auto p = camera_point(camera, corner.position);
        if (!std::isfinite(corner.u) || !std::isfinite(corner.v))
          throw std::runtime_error("Non-finite camera UV");
        polygon.push_back({p.x, p.y, p.z, corner.u, corner.v});
      }
      // Sutherland-Hodgman in camera space. Clip before division and integer
      // casts.
      for (int plane = 0; plane < 6 && !polygon.empty(); ++plane) {
        auto distance = [&](Vertex p) {
          switch (plane) {
          case 0:
            return p.z - 1.;
          case 1:
            return 20000. - p.z;
          case 2:
            return p.x + p.z * width * .5 / fx;
          case 3:
            return p.z * width * .5 / fx - p.x;
          case 4:
            return p.y + p.z * height * .5 / fy;
          default:
            return p.z * height * .5 / fy - p.y;
          }
        };
        std::vector<Vertex> clipped;
        auto previous = polygon.back();
        double dp = distance(previous);
        for (auto current : polygon) {
          double dc = distance(current);
          if ((dp >= 0) != (dc >= 0)) {
            double t = dp / (dp - dc);
            clipped.push_back({previous.x + t * (current.x - previous.x),
                               previous.y + t * (current.y - previous.y),
                               previous.z + t * (current.z - previous.z),
                               previous.u + t * (current.u - previous.u),
                               previous.v + t * (current.v - previous.v)});
          }
          if (dc >= 0)
            clipped.push_back(current);
          previous = current;
          dp = dc;
        }
        polygon = std::move(clipped);
      }
      for (size_t n = 2; n < polygon.size(); ++n) {
        std::array<Vertex, 3> vertices{polygon[0], polygon[n - 1], polygon[n]};
        for (auto &p : vertices) {
          p.x = width * .5 + p.x * fx / p.z;
          p.y = height * .5 - p.y * fy / p.z;
        }
        double area =
            edge(vertices[0], vertices[1], vertices[2].x, vertices[2].y);
        if (std::abs(area) < 1e-9)
          continue;
        int xmin = int(
                std::clamp(std::floor(std::min(
                               {vertices[0].x, vertices[1].x, vertices[2].x})),
                           0., double(width) - 1)),
            xmax =
                int(std::clamp(std::ceil(std::max({vertices[0].x, vertices[1].x,
                                                   vertices[2].x})),
                               0., double(width) - 1));
        int ymin = int(
                std::clamp(std::floor(std::min(
                               {vertices[0].y, vertices[1].y, vertices[2].y})),
                           0., double(height) - 1)),
            ymax =
                int(std::clamp(std::ceil(std::max({vertices[0].y, vertices[1].y,
                                                   vertices[2].y})),
                               0., double(height) - 1));
        auto a = triangle.corners[0].position, b = triangle.corners[1].position,
             c = triangle.corners[2].position;
        double ax = double(b.x) - a.x, ay = double(b.y) - a.y,
               az = double(b.z) - a.z, bx = double(c.x) - a.x,
               by = double(c.y) - a.y, bz = double(c.z) - a.z;
        double nx = ay * bz - az * by, ny = az * bx - ax * bz,
               nz = ax * by - ay * bx,
               length = std::sqrt(nx * nx + ny * ny + nz * nz);
        double light = length > 1e-5
                           ? .4 + .6 * std::abs((nx * .3 + ny * .8 + nz * .5) /
                                                (length * .98994949))
                           : .5;
        const Texture *texture = triangle.texture == 0xffff
                                     ? nullptr
                                     : &scene.textures[triangle.texture];
        for (int y = ymin; y <= ymax; ++y)
          for (int x = xmin; x <= xmax; ++x) {
            double w0 = edge(vertices[1], vertices[2], x + .5, y + .5) / area,
                   w1 = edge(vertices[2], vertices[0], x + .5, y + .5) / area,
                   w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0)
              continue;
            w0 /= vertices[0].z;
            w1 /= vertices[1].z;
            w2 /= vertices[2].z;
            double reciprocal = w0 + w1 + w2, z = 1 / reciprocal;
            size_t pixel = size_t(y) * width + x;
            if (z >= depth[pixel])
              continue;
            double u = (w0 * vertices[0].u + w1 * vertices[1].u +
                        w2 * vertices[2].u) /
                       reciprocal,
                   v = (w0 * vertices[0].v + w1 * vertices[1].v +
                        w2 * vertices[2].v) /
                       reciprocal;
            u -= std::floor(u);
            v -= std::floor(v);
            const uint8_t *color = triangle.color.data();
            if (texture) {
              auto tx = std::min(texture->width - 1,
                                 uint32_t(u * texture->width)),
                   ty = std::min(texture->height - 1,
                                 uint32_t(v * texture->height));
              color = &texture->rgba[(size_t(ty) * texture->width + tx) * 4];
            }
            if (color[3] < 128)
              continue;
            depth[pixel] = z;
            for (int channel = 0; channel < 3; ++channel)
              image.rgb[pixel * 3 + channel] =
                  uint8_t(std::min(255., color[channel] * light));
          }
      }
    }
  }
  return image;
}
} // namespace ashford
