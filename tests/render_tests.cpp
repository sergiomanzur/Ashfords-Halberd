#include "render.hpp"
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool ok) {
  if (!ok)
    throw std::runtime_error("Raster check failed");
}
int main() {
  try {
    Scene scene;
    scene.textures.push_back({1, 1, {255, 0, 0, 255}});
    Triangle triangle;
    triangle.corners = {Corner{{0, 0, 0}, 0, 0}, Corner{{1, 0, 0}, 1, 0},
                        Corner{{0, 1, 0}, 0, 1}};
    scene.meshes.push_back({0, {triangle}});
    auto image = render_overview(scene, 64, 64);
    size_t colored = 0;
    for (size_t i = 0; i < image.rgb.size(); i += 3)
      if (image.rgb[i] > 80 && image.rgb[i + 1] == 0)
        ++colored;
    ck(colored > 100 && image.rgb.size() == 64 * 64 * 3);
    ck(render_overview(scene, 64, 64).rgb == image.rgb);
    scene.textures[0].rgba[3] = 0;
    auto clear = render_overview(scene, 64, 64);
    for (size_t i = 0; i < clear.rgb.size(); i += 3)
      ck(clear.rgb[i] == 18 && clear.rgb[i + 1] == 22);
    scene.textures[0].rgba[3] = 255;
    scene.textures.push_back({1, 1, {0, 0, 255, 255}});
    auto front = triangle;
    front.texture = 1;
    for (auto &corner : front.corners) {
      corner.position.x += .25f;
      corner.position.y += .25f;
      corner.position.z += .25f;
    }
    scene.meshes.push_back({1, {front}});
    auto covered = render_overview(scene, 64, 64);
    size_t blue = 0;
    for (size_t i = 0; i < covered.rgb.size(); i += 3) {
      ck(covered.rgb[i] < 80);
      if (covered.rgb[i + 2] > 80)
        ++blue;
    }
    ck(blue > 100);
    std::swap(scene.meshes[0], scene.meshes[1]);
    ck(render_overview(scene, 64, 64).rgb == covered.rgb);
    bool rejected = false;
    try {
      render_overview(scene, 0, 64);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    ck(rejected);
    std::cout << "Raster boundaries passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
