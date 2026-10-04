#include "render.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
Scene basic() {
  Scene s;
  s.ambient.emplace();
  s.lighting.emplace();
  s.lighting->has_direction = true;
  s.lighting->direction = {{0, 0, -1}, {1, 1, 1}};
  s.textures.push_back({1, 1, {100, 200, 250, 255}});
  Triangle t;
  t.corners = {Corner{{-.15f, -.15f, -.5f}, 0, 0, {0, 0, .1f}},
               Corner{{1, -1, -3}, 0, 0, {0, 0, 1}},
               Corner{{-1, 1, -3}, 0, 0, {0, 0, 1}}};
  s.meshes.push_back({512, {t}});
  return s;
}
AmbientMaterial center(const Scene &s) {
  Camera c;
  c.perspective = 16384;
  const auto image = render_camera(s, c, 64, 48);
  const size_t p = (24 * 64 + 32) * 3;
  return {image.rgb[p], image.rgb[p + 1], image.rgb[p + 2]};
}
void close(AmbientMaterial actual, AmbientMaterial expected, const char *why) {
  for (size_t c = 0; c < 3; ++c)
    require(std::abs(int(actual[c]) - expected[c]) <= 1, why);
}
void clipped() {
  auto s = basic();
  s.fog = NativeFog{true, 0x151521, generate_fog_table(0, -10)};
  // Analytic near-plane intersections, rather than another call to the clipper.
  struct Sample {
    double x, y, rgb, fog;
  };
  const float fa = fog_power(*s.fog, 2.f), fb = fog_power(*s.fog, 1.f / 3.f);
  const double clipped_fog = std::trunc(double(fa) + .2 * (double(fb) - fa));
  const double distant_fog = std::trunc(double(fb));
  const std::array<Sample, 4> polygon{
      {{32 - .32 * 37.568, 24 - .08 * 32, 35, clipped_fog},
       {32 + .08 * 37.568, 24 + .32 * 32, 35, clipped_fog},
       {32 + 37.568 / 3, 24 + 32. / 3, 128, distant_fog},
       {32 - 37.568 / 3, 24 - 32. / 3, 128, distant_fog}}};
  AmbientMaterial expected{};
  bool found = false;
  for (auto indices :
       std::array<std::array<int, 3>, 2>{{{0, 1, 2}, {0, 2, 3}}}) {
    const auto a = polygon[indices[0]], b = polygon[indices[1]],
               c = polygon[indices[2]];
    const double determinant =
        (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    const double wa =
        ((b.y - c.y) * (32.5 - c.x) + (c.x - b.x) * (24.5 - c.y)) / determinant;
    const double wb =
        ((c.y - a.y) * (32.5 - c.x) + (a.x - c.x) * (24.5 - c.y)) / determinant;
    const double wc = 1 - wa - wb;
    if (wa < 0 || wb < 0 || wc < 0)
      continue;
    const double color = wa * a.rgb + wb * b.rgb + wc * c.rgb,
                 fog = wa * a.fog + wb * b.fog + wc * c.fog;
    const int tex[3] = {100, 200, 250}, fc[3] = {21, 21, 33};
    for (size_t channel = 0; channel < 3; ++channel)
      expected[channel] =
          uint8_t(fc[channel] +
                  (tex[channel] * color / 128. - fc[channel]) * (fog / 256.));
    found = true;
  }
  require(found, "Analytic clipped fixture must cover sample");
  close(
      center(s), expected,
      "Unequal RGB and fog must interpolate at actual clipping intersections");
  auto flat = s;
  flat.fog.reset();
  flat.meshes[0].triangles[0].strip_flags = 0x20;
  require(center(flat) == AmbientMaterial{100, 200, 250},
          "Flat source strip uses last submitted vertex after clipping");
  auto foreground = basic().meshes[0].triangles[0];
  foreground.corners = {Corner{{-1, -1, -1.01f}, 0, 0, {0, 0, .5f}},
                        Corner{{1, -1, -1.01f}, 0, 0, {0, 0, .5f}},
                        Corner{{0, 1, -1.01f}, 0, 0, {0, 0, .5f}}};
  // Submit the nearer polygon first; the later clipped background cannot win.
  s.meshes[0].triangles.insert(s.meshes[0].triangles.begin(), foreground);
  const double f = std::trunc(fog_power(*s.fog, 1.f / 1.01f));
  const int tex[3] = {50, 100, 125}, fc[3] = {21, 21, 33};
  for (size_t channel = 0; channel < 3; ++channel)
    expected[channel] =
        uint8_t(fc[channel] + (tex[channel] - fc[channel]) * (f / 256.));
  close(center(s), expected,
        "Foreground depth must preserve its own native color and fog");
}
void policy() {
  RoomRuntime r;
  r.scene = basic();
  r.scenario.ambient = r.scene.ambient;
  auto lighting = *r.scene.lighting;
  lighting.direction.color = {2, 2, 2};
  lighting.single_color = {3, 4, 5};
  lighting.single_vector = {0, 0, 1};
  lighting.single_intensity = 1;
  r.scenario.lighting = lighting;
  r.scenario.drawing_lighting = lighting;
  r.scenario.lights.emplace();
  ScenarioEntity normal;
  normal.flags = 1;
  normal.id = 1;
  normal.rendered_model = basic();
  auto half = normal;
  half.id = 1000;
  auto special = normal;
  special.type = 5;
  special.angular_speed = 1;
  r.scenario.objects = {normal, half, special};
  auto item = normal;
  item.model_flags = 0x80;
  item.angular_speed = 5;
  r.scenario.items = {item};
  r.player_scene = basic();
  r.player_work.draw_type = 1;
  auto scene = compose_room_runtime(r, 0x2f, 0);
  require(scene.meshes.size() == 6,
          "Original room/object/half/special/item/player order");
  const float values[] = {2, 2, 1, 1, .5f, 1};
  for (size_t i = 0; i < 6; ++i)
    require(scene.meshes[i].lighting &&
                scene.meshes[i].lighting->direction.color[0] == values[i],
            "Original drawing group changes actual per-mesh light snapshot");
  require(scene.meshes[4].lighting->single_color ==
              std::array<float, 3>{3, 4, 5},
          "Pickup highlight leaves single channels unchanged");
  require(scene.meshes[5].draw_mode == 3 &&
              scene.ending_lighting->single_color[0] == 1.5f,
          "Entity draw mapping and unconditional main half light");
  r.scenario.flags[10][0] |= 0x20;
  r.scenario.drawing_lighting->direction.color = {6, 6, 6};
  scene = compose_room_runtime(r, 0x2f, 0);
  require(scene.meshes[1].lighting->direction.color[0] == 6 &&
              scene.meshes[2].lighting->direction.color[0] == 6 &&
              scene.meshes[5].lighting->direction.color[0] == 1,
          "ST20 preserves object state but main half light still runs");
  r.scenario.flags[10][0] &= ~0x20u;
  r.scenario.flags[11][0] |= 0x80;
  r.scenario.lights->normal.resize(4);
  auto &flash = r.scenario.lights->normal[0];
  flash.flags = 3;
  flash.type = 101;
  flash.angular_speed = 1;
  scene = compose_room_runtime(r, 0x2f, 0);
  require(scene.ending_light_work->normal[0].counters[0] == 1024 &&
              flash.counters[0] == 0,
          "Four original easy draw calls advance copied clock for valid-frame "
          "publication");
  require(scene.meshes[0].draw_mode == 0 && scene.meshes[1].draw_mode == 0 &&
              scene.meshes[5].draw_mode == 2,
          "GM80 switches both entity and room draw style");
}
int main(int argc, char **argv) {
  try {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "clipped")
      clipped();
    else if (mode == "policy")
      policy();
    else
      throw std::runtime_error("Unknown extra light case");
    std::cout << "PASS " << mode << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
