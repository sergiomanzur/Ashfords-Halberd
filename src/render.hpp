#pragma once
#include "scene.hpp"
namespace ashford {
struct RasterImage {
  uint32_t width = 0, height = 0;
  Bytes rgb;
};
// Development overview camera, independent of SDL or a desktop window.
RasterImage render_overview(const Scene &scene, uint32_t width,
                            uint32_t height);
} // namespace ashford
