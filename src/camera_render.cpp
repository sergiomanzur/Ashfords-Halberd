#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
namespace ashford {
namespace {
struct Vertex {
  double x, y, z, u, v, fog;
  std::array<double, 4> color{};
};
double edge(const Vertex &a, const Vertex &b, double x, double y) {
  return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
} // namespace
RasterImage render_camera(const Scene &scene, const Camera &camera,
                          uint32_t width, uint32_t height,
                          AmbientMaterial *retained_material,
                          bool *completed_fog_state,
                          LightMaterial *retained_light_material) {
  auto material =
      retained_material ? *retained_material : AmbientMaterial{255, 255, 255};
  auto light_material =
      retained_light_material ? *retained_light_material : LightMaterial{};
  if (!retained_light_material)
    light_material.ambient = material;
  if (!width || !height || width > 2048 || height > 2048 ||
      camera.perspective < 2 || camera.perspective >= 32768)
    throw std::runtime_error("Invalid camera raster parameters");
  for (const auto &tex : scene.textures)
    if (!tex.width || !tex.height ||
        tex.rgba.size() != uint64_t(tex.width) * tex.height * 4)
      throw std::runtime_error("Invalid camera texture");
  std::optional<NativeFog> authored_fog;
  if (!scene.fog && camera.fog)
    authored_fog =
        NativeFog{true, camera.fog->color & 0xffffff,
                  generate_fog_table(camera.fog->near_z, camera.fog->far_z)};
  const auto *fog = scene.fog      ? &*scene.fog
                    : authored_fog ? &*authored_fog
                                   : nullptr;
  bool drawing_fog = fog                   ? fog->enabled
                     : completed_fog_state ? *completed_fog_state
                                           : false;
  NativeFog sampled_fog = fog ? *fog : NativeFog{};
  sampled_fog.enabled = true;
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
  constexpr double near_clip = 1, far_clip = 20000;

  // system.c njClipZ(-2,-20000), CalcPs2ZbuffAB and Ps2AddPrim3DEx.
  const float source_a = (-65536.f * -2.f) / (-20000.f + 2.f);
  const float source_b = (65536.f * -2.f * -20000.f) / (-20000.f + 2.f);
  auto source_depth = [&](float z) {
    return std::clamp((-source_b * (1.f / z)) - source_a, 0.f, 65534.f);
  };
  auto depth_bucket = [](float sum, size_t count) {
    const float average = sum * (1.f / float(count));
    return uint16_t(std::min(4095u, uint32_t(std::nearbyint(average)) >> 4));
  };
  struct Primitive {
    std::vector<Vertex> polygon;
    const Triangle *triangle;
    AmbientMaterial material;
    LightMaterial light_material;
    std::array<float, 3> ambient;
    bool lighting, use_fog, deferred;
    uint16_t bucket;
  };
  std::vector<Primitive> deferred;
  auto draw = [&](const Primitive &primitive) {
    const auto &polygon = primitive.polygon;
    const auto &triangle = *primitive.triangle;
    const auto &material = primitive.material;
    const auto &light_material = primitive.light_material;
    const auto &ambient = primitive.ambient;
    const bool lighting = primitive.lighting, use_fog = primitive.use_fog;
    for (size_t n = 2; n < polygon.size(); ++n) {
      std::array<Vertex, 3> vertices{polygon[0], polygon[n - 1], polygon[n]};
      for (auto &p : vertices) {
        p.x = width * .5 + p.x * fx / p.z;
        p.y = height * .5 - p.y * fy / p.z;
        // GS vertex F is an unsigned byte after the source float conversion.
        p.fog = std::trunc(std::clamp(p.fog, 0., 255.));
        if (lighting)
          for (auto &channel : p.color)
            channel = std::trunc(std::clamp(channel, 0., 255.));
      }
      double area =
          edge(vertices[0], vertices[1], vertices[2].x, vertices[2].y);
      if (std::abs(area) < 1e-9)
        continue;
      auto top_left = [&](size_t first, size_t second) {
        const auto &a = vertices[area > 0 ? first : second];
        const auto &b = vertices[area > 0 ? second : first];
        return b.y < a.y || (b.y == a.y && b.x > a.x);
      };
      const std::array<bool, 3> included_edges{top_left(1, 2), top_left(2, 0),
                                               top_left(0, 1)};
      int xmin =
              int(std::clamp(std::floor(std::min({vertices[0].x, vertices[1].x,
                                                  vertices[2].x})),
                             0., double(width) - 1)),
          xmax =
              int(std::clamp(std::ceil(std::max({vertices[0].x, vertices[1].x,
                                                 vertices[2].x})),
                             0., double(width) - 1));
      int ymin =
              int(std::clamp(std::floor(std::min({vertices[0].y, vertices[1].y,
                                                  vertices[2].y})),
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
                 w2 =
                     lighting
                         ? edge(vertices[0], vertices[1], x + .5, y + .5) / area
                         : 1 - w0 - w1;
          if (w0 < 0 || w1 < 0 || w2 < 0)
            continue;
          // GS spans include their top/left boundary and exclude bottom/right.
          // A shared strip edge belongs to one triangle, including FB_ONLY.
          if (lighting && ((w0 == 0 && !included_edges[0]) ||
                           (w1 == 0 && !included_edges[1]) ||
                           (w2 == 0 && !included_edges[2])))
            continue;
          // Keep constant source attributes exact. Weighted sums can turn a
          // constant GS color128 into127.999..., losing a byte on conversion.
          const double fog_factor = vertices[2].fog +
                                    w0 * (vertices[0].fog - vertices[2].fog) +
                                    w1 * (vertices[1].fog - vertices[2].fog);
          std::array<double, 4> vertex_color{};
          if (lighting)
            for (size_t channel = 0; channel < 4; ++channel)
              vertex_color[channel] = vertices[2].color[channel] +
                                      w0 * (vertices[0].color[channel] -
                                            vertices[2].color[channel]) +
                                      w1 * (vertices[1].color[channel] -
                                            vertices[2].color[channel]);
          w0 /= vertices[0].z;
          w1 /= vertices[1].z;
          w2 /= vertices[2].z;
          double reciprocal = w0 + w1 + w2, z = 1 / reciprocal;
          size_t pixel = size_t(y) * width + x;
          const double fragment_depth = lighting ? -source_depth(float(z)) : z;
          if (lighting ? fragment_depth > depth[pixel]
                       : fragment_depth >= depth[pixel])
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
            auto tx =
                     std::min(texture->width - 1, uint32_t(u * texture->width)),
                 ty = std::min(texture->height - 1,
                               uint32_t(v * texture->height));
            color = &texture->rgba[(size_t(ty) * texture->width + tx) * 4];
          }
          const double source_alpha =
              lighting ? vertex_color[3] * (texture ? color[3] / 255. : 1.)
                       : color[3];
          if (lighting ? (!primitive.deferred && source_alpha < 1.)
                       : color[3] < 128)
            continue;
          // Deferred TEST uses AFAIL_FB_ONLY below128, including alpha0.
          if (!primitive.deferred || source_alpha >= 128.)
            depth[pixel] = fragment_depth;
          for (int channel = 0; channel < 3; ++channel) {
            const double base =
                scene.ambient && !texture ? 255 : color[channel];
            const double factor =
                scene.ambient
                    ? double(ambient[channel] * (material[channel] / 255.f))
                    : light;
            double shade = std::clamp(base * factor, 0., 255.);
            if (lighting)
              shade = std::clamp((texture ? color[channel] : 128) *
                                     vertex_color[channel] / 128.,
                                 0., 255.);
            if (use_fog || (lighting && fog)) {
              const double fog_color = (fog->color >> (16 - channel * 8)) & 255;
              shade = fog_color + (shade - fog_color) * (fog_factor / 256.);
            }
            if (lighting && (triangle.strip_flags & 8)) {
              const auto reg = light_material.gs_alpha;
              const double destination = image.rgb[pixel * 3 + channel];
              auto operand = [&](unsigned select) {
                return select == 0 ? shade : select == 1 ? destination : 0.;
              };
              const unsigned select = (reg >> 4) & 3;
              if (select == 1 || select == 3)
                throw std::runtime_error(
                    "Native blending requires destination alpha binding");
              const double alpha = select == 2 ? ((reg >> 32) & 255) / 128.
                                               : source_alpha / 128.;
              shade = (operand(reg & 3) - operand((reg >> 2) & 3)) * alpha +
                      operand((reg >> 6) & 3);
            }
            image.rgb[pixel * 3 + channel] =
                uint8_t(std::clamp(shade, 0., 255.));
          }
        }
    }
  };
  Camera normal_camera = camera;
  normal_camera.position = {};
  normal_camera.distance = 0;
  size_t total = 0;
  for (const auto &mesh : scene.meshes) {
    if (mesh.fog_state)
      drawing_fog = *mesh.fog_state;
    if (mesh.object < 512 &&
        (camera.hidden[mesh.object / 32] & (0x80000000u >> (mesh.object % 32))))
      continue;
    if (mesh.model_clipping && mesh.bounds) {
      const auto &bounds = *mesh.bounds;
      if (!std::isfinite(bounds.radius))
        throw std::runtime_error("Non-finite source model radius");
      if (bounds.radius > 0) {
        const auto center = camera_point(camera, bounds.center);
        const double radius = bounds.radius;
        if (center.z + radius < near_clip || far_clip < center.z - radius)
          continue;
        // Source sphere projection tests Center.x/y without multiplying them
        // by the aspect; only projected radius receives that scale.
        const double projection = std::abs(fy / center.z);
        const double rw = radius * projection * 1.174;
        const double rh = radius * projection;
        const double sx = center.x * projection, sy = center.y * projection;
        if (sx + rw < -double(width) * .5 || width * .5 < sx - rw ||
            sy + rh < -double(height) * .5 || height * .5 < sy - rh)
          continue;
      }
    }
    const auto ambient =
        scene.ambient ? selected_ambient(*scene.ambient, mesh.ambient_target)
                      : std::array<float, 3>{1, 1, 1};
    const bool use_fog = fog && drawing_fog && mesh.fog_enabled;
    const auto *lighting = mesh.lighting    ? &*mesh.lighting
                           : scene.lighting ? &*scene.lighting
                                            : nullptr;
    std::unordered_map<const std::vector<Corner> *, uint16_t> strip_buckets;
    for (const auto &triangle : mesh.triangles) {
      if (triangle.ambient_material)
        material = *triangle.ambient_material;
      apply_light_material(light_material, triangle.light_material);
      if (++total > 2000000 || (triangle.texture != 0xffff &&
                                triangle.texture >= scene.textures.size()))
        throw std::runtime_error("Invalid camera scene");
      // Source Single rejects only when every camera normal Z is positive
      // before the screen-depth flip. Material commands already applied.
      if (lighting && mesh.draw_mode >= 2 && !(triangle.strip_flags & 0x10) &&
          std::all_of(triangle.corners.begin(), triangle.corners.end(),
                      [&](const Corner &corner) {
                        return camera_point(normal_camera, corner.normal).z < 0;
                      }))
        continue;
      std::vector<Vertex> polygon;
      for (auto corner : triangle.corners) {
        auto p = camera_point(camera, corner.position);
        if (!std::isfinite(corner.u) || !std::isfinite(corner.v))
          throw std::runtime_error("Non-finite camera UV");
        Vertex vertex{
            p.x,      p.y,
            p.z,      corner.u,
            corner.v, use_fog ? fog_power(sampled_fog, 1.f / p.z) : 255.f};
        if (lighting) {
          const auto rgba = light_vertex_rgba(
              *lighting, camera, corner.position, corner.normal, light_material,
              ambient, mesh.draw_mode, triangle.strip_flags,
              triangle.texture != 0xffff);
          std::copy(rgba.begin(), rgba.end(), vertex.color.begin());
        }
        polygon.push_back(vertex);
      }
      if (lighting && (triangle.strip_flags & 0x20))
        for (auto &vertex : polygon)
          vertex.color = polygon.back().color;
      // Sutherland-Hodgman in camera space. Clip before division and integer
      // casts.
      for (int plane = 0; plane < 6 && !polygon.empty(); ++plane) {
        auto distance = [&](Vertex p) {
          switch (plane) {
          case 0:
            return p.z - near_clip;
          case 1:
            return far_clip - p.z;
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
            Vertex generated{previous.x + t * (current.x - previous.x),
                             previous.y + t * (current.y - previous.y),
                             previous.z + t * (current.z - previous.z),
                             previous.u + t * (current.u - previous.u),
                             previous.v + t * (current.v - previous.v),
                             previous.fog + t * (current.fog - previous.fog)};
            for (size_t channel = 0; channel < 4; ++channel)
              generated.color[channel] =
                  previous.color[channel] +
                  t * (current.color[channel] - previous.color[channel]);
            clipped.push_back(generated);
          }
          if (dc >= 0)
            clipped.push_back(current);
          previous = current;
          dp = dc;
        }
        polygon = std::move(clipped);
      }

      Primitive primitive{std::move(polygon),
                          &triangle,
                          material,
                          light_material,
                          ambient,
                          lighting != nullptr,
                          use_fog,
                          false,
                          0};
      // Easy Double and Simple Double enter the source ordering table;
      // Simple Single uses the immediate VU1 path.
      primitive.deferred =
          lighting && (triangle.strip_flags & 8) &&
          (mesh.draw_mode < 2 || (triangle.strip_flags & 0x10));
      if (primitive.deferred) {
        if (triangle.source_strip) {
          const auto *group = triangle.source_strip.get();
          auto found = strip_buckets.find(group);
          if (found == strip_buckets.end()) {
            if (group->size() < 3 || group->size() > 32768)
              throw std::runtime_error(
                  "Invalid source transparent strip extent");
            float sum = 0;
            for (const auto &corner : *group)
              sum += source_depth(camera_point(camera, corner.position).z);
            found =
                strip_buckets.emplace(group, depth_bucket(sum, group->size()))
                    .first;
          }
          primitive.bucket = found->second;
        } else {
          float sum = 0;
          for (const auto &corner : triangle.corners)
            sum += source_depth(camera_point(camera, corner.position).z);
          primitive.bucket = depth_bucket(sum, triangle.corners.size());
        }
        deferred.push_back(std::move(primitive));
      } else
        draw(primitive);
    }
    if (mesh.ending_ambient_material)
      material = *mesh.ending_ambient_material;
    apply_light_material(light_material, mesh.ending_light_material);
  }
  // Source Ps2DrawOTag visits4096 buckets in ascending order, preserving
  // submission order within a bucket. All opaque work has now finished.
  std::stable_sort(deferred.begin(), deferred.end(),
                   [](const Primitive &a, const Primitive &b) {
                     return a.bucket < b.bucket;
                   });
  for (const auto &primitive : deferred)
    draw(primitive);
  if (retained_material)
    *retained_material = material;
  if (completed_fog_state)
    *completed_fog_state = scene.ending_fog_state.value_or(drawing_fog);
  if (retained_light_material)
    *retained_light_material = light_material;
  return image;
}
} // namespace ashford
