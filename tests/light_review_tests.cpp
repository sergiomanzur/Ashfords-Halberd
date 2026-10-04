#include "render.hpp"
#include "runtime.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ashford;
namespace {
void check(bool ok, const char *why) {
  if (!ok)
    throw std::runtime_error(why);
}
Scene scene() {
  Scene s;
  s.ambient.emplace();
  s.lighting.emplace();
  s.lighting->has_direction = true;
  s.lighting->direction = {{0, 0, -1}, {1, 1, 1}};
  s.textures = {{1, 1, {200, 0, 0, 255}},
                {1, 1, {0, 200, 0, 255}},
                {1, 1, {0, 0, 200, 255}}};
  return s;
}
Mesh polygon(float z, uint16_t texture, bool transparent = false) {
  Triangle t;
  t.texture = texture;
  t.strip_flags = transparent ? 9 : 1;
  t.corners = {Corner{{-2, -2, -z}, 0, 0, {0, 0, 1}},
               Corner{{2, -2, -z}, 0, 0, {0, 0, 1}},
               Corner{{0, 2, -z}, 0, 0, {0, 0, 1}}};
  t.light_material.diffuse =
      std::array<uint8_t, 4>{255, 255, 255, uint8_t(transparent ? 128 : 255)};
  t.light_material.gs_alpha = 0x44;
  return Mesh{512, {t}};
}
AmbientMaterial pixel(const Scene &s) {
  Camera c;
  c.perspective = 16384;
  const auto image = render_camera(s, c, 64, 48);
  const size_t p = (24 * 64 + 32) * 3;
  return {image.rgb[p], image.rgb[p + 1], image.rgb[p + 2]};
}
void paths() {
  NativeLighting l;
  l.has_direction = true;
  l.direction = {{0, 0, -1}, {1, 1, 1}};
  LightMaterial m;
  Camera c;
  auto color =
      light_vertex_rgba(l, c, {0, 0, -3}, {0, 0, 1}, m, {0, 0, 0}, 3, 4, true);
  check(color[0] == 128 && color[1] == 128 && color[2] == 128,
        "Actual textured VU1 function5 uses diffuse/specular constants");
  l.single_vector = {0, 0, 1};
  l.single_color = {1, 1, 1};
  l.single_intensity = .9f;
  color =
      light_vertex_rgba(l, c, {0, 0, -3}, {0, 0, 1}, m, {0, 0, 0}, 2, 0, true);
  // VSM12A0 starts the accumulator at i+a+1, before seventeen MULs.
  float power = .9f + 1.f;
  for (int i = 0; i < 17; ++i)
    power *= .9f;
  const float expected = .9f * 128.f + power * 31.f;
  check(color[0] == expected,
        "Actual VU1 function10 retains source power accumulator seed");
}
void shared_edges() {
  for (bool horizontal : {false, true})
    for (bool reversed : {false, true}) {
      auto s = scene();
      auto mesh = polygon(3, 0, true);
      Corner a{{-1, -1, -3}, 0, 0, {0, 0, 1}}, b{{1, -1, -3}, 0, 0, {0, 0, 1}},
          c{{-1, 1, -3}, 0, 0, {0, 0, 1}}, d{{1, 1, -3}, 0, 0, {0, 0, 1}};
      if (horizontal) {
        a.position = {0, 1, -3};
        b.position = {-1, 0, -3};
        c.position = {1, 0, -3};
        d.position = {0, -1, -3};
      }
      mesh.triangles[0].corners = {a, b, c};
      mesh.triangles.push_back(mesh.triangles[0]);
      mesh.triangles[1].corners = {c, b, d};
      const auto strip = std::make_shared<const std::vector<Corner>>(
          std::vector<Corner>{a, b, c, d});
      for (auto &t : mesh.triangles) {
        t.source_strip = strip;
        if (reversed)
          std::swap(t.corners[0], t.corners[1]);
      }
      s.meshes = {mesh, polygon(4, 2)};
      Camera camera;
      camera.perspective = 16384;
      const auto image = render_camera(s, camera, 65, 49);
      const size_t center = (24 * 65 + 32) * 3;
      check(image.rgb[center] == 100 && image.rgb[center + 1] == 0 &&
                image.rgb[center + 2] == 100,
            "Shared source strip edges blend exactly once for both windings");
    }
}

void transparency() {
  shared_edges();
  auto s = scene();
  s.meshes = {polygon(2, 0, true), polygon(4, 2)};
  check(pixel(s) == AmbientMaterial{100, 0, 100},
        "Source deferred foreground blends over later submitted opaque "
        "background");
  s.meshes = {polygon(2, 0, true), polygon(3, 1, true), polygon(4, 2)};
  check(pixel(s) == AmbientMaterial{100, 50, 50},
        "Source transparent layers draw from far to near regardless of "
        "submission order");
  s.meshes = {polygon(2, 0), polygon(2, 2)};
  check(pixel(s) == AmbientMaterial{0, 0, 200},
        "Native GS GEQUAL accepts equal-depth submission");
  auto zero = polygon(2, 0, true);
  zero.triangles[0].light_material.diffuse =
      std::array<uint8_t, 4>{255, 255, 255, 0};
  zero.triangles[0].light_material.gs_alpha = 0x8000000068ull;
  s.meshes = {zero, polygon(4, 2)};
  check(pixel(s) == AmbientMaterial{200, 0, 200},
        "Deferred alpha-fail framebuffer-only path keeps zero-alpha fixed "
        "blending");
  // A fourth strip vertex outside the viewport still contributes to OT depth.
  auto grouped = polygon(2, 0, true);
  auto group = std::vector<Corner>(grouped.triangles[0].corners.begin(),
                                   grouped.triangles[0].corners.end());
  group.push_back(Corner{{10000, 10000, -100}, 0, 0, {0, 0, 1}});
  grouped.triangles[0].source_strip =
      std::make_shared<const std::vector<Corner>>(std::move(group));
  s.meshes = {grouped, polygon(2.5f, 1, true), polygon(4, 2)};
  check(pixel(s) == AmbientMaterial{50, 100, 50},
        "Source transparent ordering retains the complete strip depth average");
  auto cover = [](float z, uint16_t texture, bool transparent) {
    auto m = polygon(z, texture, transparent);
    for (auto &corner : m.triangles[0].corners) {
      corner.position.x *= z;
      corner.position.y *= z;
    }
    return m;
  };
  // Both depths land in source OT bucket8; insertion order takes priority.
  s.meshes = {cover(1000, 0, true), cover(1001, 1, true),
              cover(2000, 2, false)};
  check(pixel(s) == AmbientMaterial{50, 100, 50},
        "Equal source buckets are stable and low alpha does not write depth");
  for (size_t i = 0; i < 2; ++i)
    s.meshes[i].triangles[0].light_material.diffuse =
        std::array<uint8_t, 4>{255, 255, 255, 255};
  check(pixel(s) == AmbientMaterial{200, 0, 0},
        "Deferred alpha128 writes depth and rejects a later farther fragment");
}
void single() {
  auto s = scene();
  auto mesh = polygon(3, 0);
  mesh.draw_mode = 3;
  s.meshes = {mesh};
  check(pixel(s) == AmbientMaterial{18, 22, 28},
        "Source Single rejects three positive camera normal Z values");
  auto retained_scene = s;
  retained_scene.meshes[0].triangles[0].light_material.ambient =
      AmbientMaterial{11, 22, 33};
  LightMaterial retained;
  Camera c;
  c.perspective = 16384;
  render_camera(retained_scene, c, 64, 48, nullptr, nullptr, &retained);
  check(retained.ambient == AmbientMaterial{11, 22, 33},
        "Single rejection retains preceding material commands");
  c.position.z = -6;
  c.angles[1] = 32768;
  const auto turned = render_camera(s, c, 64, 48);
  const size_t center = (24 * 64 + 32) * 3;
  check(turned.rgb[center] == 200 && turned.rgb[center + 1] == 0 &&
            turned.rgb[center + 2] == 0,
        "Single rejection uses rotated camera normals independently of "
        "translation");
  s.meshes[0].triangles[0].corners[0].normal.z = -1;
  check(pixel(s) == AmbientMaterial{200, 0, 0},
        "Mixed source normal signs keep Single primitive");
  s.meshes[0] = mesh;
  s.meshes[0].triangles[0].strip_flags |= 0x10;
  check(pixel(s) == AmbientMaterial{200, 0, 0},
        "Source double-sided bit bypasses Single rejection");
  s.meshes[0] = mesh;
  s.meshes[0].draw_mode = 1;
  check(pixel(s) == AmbientMaterial{200, 0, 0},
        "Easy Double does not apply Single rejection");
}
void put(Bytes &b, size_t p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(value >> (i * 8));
}
void bind_control(ScenarioState &s, const Bytes &code) {
  Bytes room(1209 + code.size());
  put(room, 0, 0x41200000);
  put(room, 16, 128);
  put(room, 28, 1200);
  put(room, 1200, 8);
  put(room, 1204, 9);
  room[1208] = 0;
  std::copy(code.begin(), code.end(), room.begin() + 1209);
  s.scenario = std::make_shared<const RoomScenario>(decode_room_scenario(room));
}
void events() {
  for (bool initially_enabled : {true, false}) {
    RoomRuntime r;
    r.scene = scene();
    r.scene.meshes = {polygon(3, 0)};
    // Use lit rather than ignore-lighting flags for the actual frame check.
    r.scene.meshes[0].triangles[0].strip_flags = 0;
    r.scenario.ambient = r.scene.ambient;
    r.scenario.flags[12][0] = 0x80;
    r.scenario.lights.emplace();
    r.scenario.lights->normal.resize(5);
    auto &light = r.scenario.lights->normal[4];
    light.flags = initially_enabled ? 3 : 2;
    light.type = 5;
    light.angular_speed = 4;
    light.source = 2;
    light.vector = {0, 0, -1};
    light.color = {2, 2, 2};
    r.cuts.resize(1);
    r.cuts[0].variants[0].base.perspective = 16384;
    r.world.cut = 0;
    bind_control(r.scenario,
                 Bytes{53, uint8_t(initially_enabled ? 1 : 0), 4, 0, 0});
    const auto seed = r.scenario.random_seed;
    ScenarioState reference_rng;
    reference_rng.random_seed = seed;
    scenario_random(reference_rng);
    tick_room_runtime(r, PlayerPackage{}, PlayerTranslationTables{}, 0);
    auto composed = compose_room_runtime(r, 0x20, 0);
    const auto first = pixel(composed);
    check(r.scenario.random_seed ==
                  (initially_enabled ? reference_rng.random_seed : seed) &&
              (initially_enabled ? first[0] > 0 : first[0] == 0),
          "Pre-event light consumes original current-frame RNG and pixels");
    const auto first_counter = r.scenario.lights->normal[4].counters[0];
    check(initially_enabled ? first_counter > 0 : first_counter == 0,
          "Pre-event controller retains original current counter");
    // RuntimeView publishes successful copied drawing work in the same way.
    r.scenario.lights = composed.ending_light_work;
    r.scenario.drawing_lighting = composed.ending_lighting;
    const auto first_seed = r.scenario.random_seed;
    tick_room_runtime(r, PlayerPackage{}, PlayerTranslationTables{}, 0);
    const auto second = pixel(compose_room_runtime(r, 0x20, 0));
    check(initially_enabled
              ? (second[0] == 0 && r.scenario.random_seed == first_seed)
              : (second[0] > 0 && r.scenario.random_seed != first_seed),
          "Event mutation survives drawing publication and takes effect next "
          "tick");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "paths")
      paths();
    else if (mode == "transparency")
      transparency();
    else if (mode == "single")
      single();
    else if (mode == "events")
      events();
    else
      throw std::runtime_error("Unknown lighting correction case");
    std::cout << "PASS " << mode << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
