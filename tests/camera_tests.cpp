#include "camera.hpp"
#include "render.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool ok) {
  if (!ok)
    throw std::runtime_error("Camera check failed");
}
template <class F> void bad(F f) {
  bool fail = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    fail = true;
  }
  ck(fail);
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
void flt(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
int main() {
  try {
    Bytes b(1812);
    put(b, 0, 0x41200000);
    put(b, 16, 128);
    put(b, 128, 1132);
    put(b, 256, 1);
    flt(b, 1132 + 20, 10);
    flt(b, 1132 + 28, 20);
    for (size_t v = 0; v < 2; ++v) {
      auto p = 1132 + 32 + v * 196;
      put(b, p + 84, 8192);
    }
    auto cuts = decode_room_cuts(b);
    ck(cuts.size() == 1);
    auto camera = resolve_camera(cuts[0], 0);
    ck(camera.perspective == 8192);
    auto point = camera_point(camera, {0, 0, -10});
    ck(point.z == 10 && point.x == 0);
    Camera pose;
    pose.position = {1, 2, 3};
    pose.distance = 5;
    pose.angles = {16384, 16384, 16384};
    auto rotated = camera_point(pose, {11, 22, 33});
    ck(std::abs(rotated.x + 10) < .0001 && std::abs(rotated.y - 30) < .0001 &&
       std::abs(rotated.z + 15) < .0001);
    put(b, 1164 + 84, 0x3c3c3c3c);
    b[1164] = 32;
    cuts = decode_room_cuts(b);
    ck(resolve_camera(cuts[0], 0).perspective == 10922);
    put(b, 1164 + 88, 0x10000000);
    cuts = decode_room_cuts(b);
    ck(resolve_camera(cuts[0], 0).hidden[0] == 0x10000000);
    bad([&] { resolve_camera(cuts[0], 2); });
    put(b, 128, 1800);
    bad([&] { decode_room_cuts(b); });
    Scene scene;
    scene.textures.push_back({1, 1, {255, 0, 0, 255}});
    Triangle t;
    t.corners = {Corner{{-1, -1, -3}, 0, 0}, Corner{{1, -1, -3}, 1, 0},
                 Corner{{0, 1, -3}, .5, 1}};
    scene.meshes.push_back({3, {t}});
    Camera fixed;
    fixed.perspective = 16384;
    auto im = render_camera(scene, fixed, 64, 48);
    size_t pixels = 0;
    for (size_t i = 0; i < im.rgb.size(); i += 3)
      if (im.rgb[i] > 80)
        ++pixels;
    ck(pixels > 100);
    scene.meshes[0].initially_hidden = true;
    ck(render_camera(scene, fixed, 64, 48).rgb == im.rgb);
    scene.meshes[0].triangles[0].corners[2].position.z = -.1f;
    auto clipped = render_camera(scene, fixed, 64, 48);
    size_t visible = 0;
    for (size_t i = 0; i < clipped.rgb.size(); i += 3)
      if (clipped.rgb[i] > 80)
        ++visible;
    ck(visible > 100);
    scene.meshes[0].triangles[0] = t;
    Scene varying;
    varying.textures.push_back({2, 1, {255, 0, 0, 255, 0, 0, 255, 255}});
    Triangle perspective;
    perspective.corners = {Corner{{float(-.8 * 2 / 1.174), -1.2f, -2}, 0, 0},
                           Corner{{float(.8 * 8 / 1.174), -4.8f, -8}, 1, 0},
                           Corner{{0, 4.8f, -8}, 1, 0}};
    varying.meshes.push_back({0, {perspective}});
    auto corrected = render_camera(varying, fixed, 64, 48);
    auto sample = (24 * 64 + 32) * 3;
    ck(corrected.rgb[sample] > 80 && corrected.rgb[sample + 2] == 0);
    fixed.hidden[0] = 0x10000000;
    auto hidden = render_camera(scene, fixed, 64, 48);
    for (size_t i = 0; i < hidden.rgb.size(); i += 3)
      ck(hidden.rgb[i] == 18);
    fixed.hidden[0] = 0;
    for (auto &c : scene.meshes[0].triangles[0].corners)
      c.position.z = 3;
    auto behind = render_camera(scene, fixed, 64, 48);
    for (size_t i = 0; i < behind.rgb.size(); i += 3)
      ck(behind.rgb[i] == 18);
    fixed.perspective = 0;
    bad([&] { render_camera(scene, fixed, 64, 48); });
    std::cout << "Camera boundaries passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
