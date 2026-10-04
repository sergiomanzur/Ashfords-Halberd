#pragma once
#include "camera.hpp"
#include "scene.hpp"
namespace ashford {
struct RasterImage {
  uint32_t width = 0, height = 0;
  Bytes rgb;
};
// Development overview camera, independent of SDL or a desktop window.
RasterImage render_overview(const Scene &scene, uint32_t width,
                            uint32_t height);
// Retain Ninja ambient material across models/frames in the caller's renderer
// session; invalid frames do not publish a replacement material value.
RasterImage render_camera(const Scene &scene, const Camera &camera,
                          uint32_t width, uint32_t height,
                          AmbientMaterial *retained_material = nullptr,
                          bool *completed_fog_state = nullptr);
} // namespace ashford
