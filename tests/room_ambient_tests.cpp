#include "ambient.hpp"
#include "render.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool value, const char *why) {
  if (!value)
    throw std::runtime_error(why);
}
template <class F> void bad(F action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Malformed ambient input must reject");
}
void put(Bytes &b, size_t p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(value >> (i * 8));
}
void real(Bytes &b, size_t p, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, 4);
  put(b, p, bits);
}
Bytes room_fixture() {
  Bytes b(1200);
  real(b, 0, 10);
  put(b, 16, 128);
  const uint8_t selectors[] = {3, 1, 2, 0};
  const float colors[4][3] = {
      {.125f, .25f, .5f}, {1, 2, 3}, {-1, 0, .7f}, {25.5f, 3.5f, 4.5f}};
  for (size_t slot = 0; slot < 4; ++slot) {
    b[128 + 0x3b8 + slot] = selectors[slot];
    for (size_t channel = 0; channel < 3; ++channel)
      real(b, 128 + 0x3bc + channel * 16 + slot * 4, colors[slot][channel]);
  }
  return b;
}
void decoding() {
  const auto b = room_fixture();
  const auto a = decode_room_ambient(b);
  ck(a.selectors == std::array<uint8_t, 4>{3, 1, 2, 0} &&
         selected_ambient(a, AmbientTarget::Room) ==
             std::array<float, 3>{25.5f, 3.5f, 4.5f} &&
         selected_ambient(a, AmbientTarget::Character) ==
             std::array<float, 3>{1, 2, 3} &&
         selected_ambient(a, AmbientTarget::Object) ==
             std::array<float, 3>{-1, 0, .7f} &&
         selected_ambient(a, AmbientTarget::Item) ==
             std::array<float, 3>{.125f, .25f, .5f},
     "Original planar float arrays and four draw selectors must decode");
  for (size_t target = 0; target < 4; ++target)
    for (uint8_t invalid : {4, 255}) {
      auto corrupt = b;
      corrupt[128 + 0x3b8 + target] = invalid;
      bad([&] { decode_room_ambient(corrupt); });
    }
  for (float invalid : {std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    auto corrupt = b;
    real(corrupt, 128 + 0x3dc + 12, invalid);
    bad([&] { decode_room_ambient(corrupt); });
  }
  auto truncated = b;
  truncated.resize(128 + 0x3eb);
  bad([&] { decode_room_ambient(truncated); });
  auto pointer = b;
  put(pointer, 16, 0xffffffff);
  bad([&] { decode_room_ambient(pointer); });
  bad([&] { selected_ambient(a, AmbientTarget(255)); });
}
void commands() {
  ScenarioState s;
  s.ambient = decode_room_ambient(room_fixture());
  const auto before = *s.ambient;
  const auto flags = s.flags;
  run_scenario(Bytes{68, 255, 2, 0, 3, 219, 55, 7, 0}, s);
  ck(s.room_case == 7 && s.flags == flags &&
         s.ambient->selectors == before.selectors &&
         s.ambient->colors[3] == std::array<float, 3>{.1f * 255, .1f * 2, 0} &&
         s.ambient->colors[0] == before.colors[0] &&
         s.ambient->colors[1] == before.colors[1] &&
         s.ambient->colors[2] == before.colors[2],
     "Original68 must retain other state, use float scaling and six bytes");
  for (const Bytes &code :
       {Bytes{68}, Bytes{68, 1, 2, 3, 0}, Bytes{68, 1, 2, 3, 4, 0},
        Bytes{68, 1, 2, 3, 255, 0}}) {
    const auto old = *s.ambient;
    bad([&] { run_scenario(code, s); });
    ck(s.ambient->colors == old.colors &&
           s.ambient->selectors == old.selectors && s.flags == flags &&
           s.room_case == 7,
       "Malformed68 must reject before room color publication");
  }
  s.ambient.reset();
  bad([&] { run_scenario(Bytes{68, 1, 2, 3, 0, 0}, s); });
}
Scene triangle_scene() {
  Scene s;
  s.textures.push_back({1, 1, {100, 200, 250, 255}});
  Triangle t;
  t.corners = {Corner{{-1, -1, -3}, 0, 0}, Corner{{1, -1, -3}, 0, 0},
               Corner{{0, 1, -3}, 0, 0}};
  s.meshes.push_back({512, {t}});
  return s;
}
std::array<uint8_t, 3> pixel(const Scene &s,
                             AmbientMaterial *material = nullptr) {
  Camera camera;
  camera.perspective = 16384;
  const auto image = render_camera(s, camera, 64, 48, material);
  const size_t p = (24 * 64 + 32) * 3;
  return {image.rgb[p], image.rgb[p + 1], image.rgb[p + 2]};
}
void near(std::array<uint8_t, 3> actual, AmbientMaterial expected) {
  for (size_t i = 0; i < 3; ++i)
    ck(std::abs(int(actual[i]) - expected[i]) <= 1,
       "Source ambient/material contribution must reach actual pixels");
}
void rendering() {
  RoomRuntime r;
  r.scene = triangle_scene();
  r.scene.ambient.emplace();
  for (auto &color : r.scene.ambient->colors)
    color = {1, 1, 1};
  r.scenario.ambient = r.scene.ambient;
  r.scenario.ambient->colors[0] = {.2f, .4f, .6f};
  near(pixel(compose_room_runtime(r, 0x20, 0)), {20, 80, 150});
  run_scenario(Bytes{68, 10, 0, 5, 0, 0, 0}, r.scenario);
  near(pixel(compose_room_runtime(r, 0x20, 0)), {100, 0, 125});
  ck(r.scene.ambient->colors[0] == std::array<float, 3>{1, 1, 1},
     "Live script rendering must not mutate the decoded initial header");
  r.scenario.ambient->colors[0] = {-1, 2, 25.5f};
  near(pixel(compose_room_runtime(r, 0x20, 0)), {0, 255, 255});
  auto scene = compose_room_runtime(r, 0x20, 0);
  scene.ambient->colors[0] = {1, 1, 1};
  scene.textures[0].rgba = {255, 255, 255, 255};
  auto &triangle = scene.meshes[0].triangles[0];
  triangle.ambient_material = AmbientMaterial{128, 64, 32};
  AmbientMaterial retained{255, 255, 255};
  near(pixel(scene, &retained), {128, 64, 32});
  ck(retained == AmbientMaterial{128, 64, 32},
     "Source ambient material must survive a completed frame");
  triangle.ambient_material.reset();
  near(pixel(scene, &retained), {128, 64, 32});
  near(pixel(scene), {255, 255, 255});
  auto across_models = scene;
  across_models.meshes[0].triangles[0].ambient_material =
      AmbientMaterial{97, 149, 211};
  across_models.meshes[0].ending_ambient_material =
      AmbientMaterial{128, 64, 32};
  for (auto &corner : across_models.meshes[0].triangles[0].corners)
    corner.position.x -= 1.2f;
  across_models.meshes.push_back(scene.meshes[0]);
  near(pixel(across_models), {128, 64, 32});
  scene.meshes[0].ending_ambient_material = AmbientMaterial{97, 149, 211};
  pixel(scene, &retained);
  ck(retained == AmbientMaterial{97, 149, 211},
     "Material command after last strip must survive into next model/frame");
  scene.meshes[0].ending_ambient_material.reset();
  triangle.texture = 0xffff;
  triangle.color = {0, 0, 0, 255};
  triangle.ambient_material = AmbientMaterial{97, 149, 211};
  near(pixel(scene), {97, 149, 211});
  scene.ambient->colors[0][1] = std::numeric_limits<float>::quiet_NaN();
  const auto old = retained;
  bad([&] { pixel(scene, &retained); });
  ck(retained == old, "Rejected lighting must not publish material state");
  r.scenario.ambient.reset();
  bad([&] { compose_room_runtime(r, 0x20, 0); });
}
Bytes material_model(uint8_t type, bool trailing = false) {
  Bytes b(640);
  b[0] = 'M';
  b[1] = 'D';
  b[2] = 'L';
  b[3] = 128;
  put(b, 4, 16 | (1u << 16));
  put(b, 16, 7);
  put(b, 20, 64);
  put(b, 60, 0xffffffff);
  put(b, 64, 0xffffffff);
  put(b, 80, 96);
  put(b, 84, 272);
  b[112] = 51;
  b[114] = 39;
  b[115] = 0;
  b[116] = 0;
  b[117] = 0;
  b[118] = 3;
  b[119] = 0;
  real(b, 208, 1);
  real(b, 244, 1);
  b[272] = 255;
  const bool diffuse = type == 19 || type == 23;
  const bool specular = type == 22 || type == 23;
  const size_t payload = 4 + (diffuse ? 4 : 0) + (specular ? 4 : 0);
  b[288] = type;
  b[290] = uint8_t(payload / 2);
  size_t a = 292;
  if (diffuse) {
    b[a + 3] = 255;
    a += 4;
  }
  b[a] = 211;
  b[a + 1] = 149;
  b[a + 2] = 97;
  b[a + 3] = 255;
  size_t p = 292 + payload;
  b[p] = 64;
  b[p + 2] = 5;
  b[p + 4] = 1;
  b[p + 6] = 3;
  b[p + 8] = 0;
  b[p + 10] = 1;
  b[p + 12] = 2;
  p += 14;
  if (trailing) {
    b[p] = 18;
    b[p + 2] = 2;
    b[p + 4] = 32;
    b[p + 5] = 64;
    b[p + 6] = 128;
    p += 8;
  }
  b[p] = 255;
  return b;
}
void materials_and_categories() {
  for (uint8_t type : {18, 19, 22, 23}) {
    auto data = material_model(type, true);
    const auto meshes = decode_model(data, 0);
    ck(meshes.size() == 1 && meshes[0].triangles.size() == 1 &&
           meshes[0].triangles[0].ambient_material ==
               AmbientMaterial{97, 149, 211} &&
           meshes[0].ending_ambient_material == AmbientMaterial{128, 64, 32},
       "Original A/Da/As/Das material chunks must retain BGR and final state");
    Scene scene = triangle_scene();
    scene.ambient.emplace();
    scene.ambient->colors[0] = {1, 1, 1};
    scene.meshes[0].triangles[0].texture = 0xffff;
    scene.meshes[0].triangles[0].color = meshes[0].triangles[0].color;
    scene.meshes[0].triangles[0].ambient_material =
        meshes[0].triangles[0].ambient_material;
    near(pixel(scene), {97, 149, 211});
    data[290] = 1;
    bad([&] { decode_model(data, 0); });
  }
  RoomRuntime r;
  r.scene = triangle_scene();
  r.player_scene = triangle_scene();
  r.scenario.ambient.emplace();
  r.scenario.ambient->selectors = {0, 1, 2, 3};
  r.scenario.ambient->colors = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}}};
  r.scene.ambient = r.scenario.ambient;
  r.scenario.objects.resize(3);
  r.scenario.items.resize(1);
  r.scenario.enemies.resize(1);
  for (auto &e : r.scenario.objects) {
    e.flags = 1;
    e.rendered_model = triangle_scene();
  }
  r.scenario.objects[0].id = 1000;
  r.scenario.objects[1].id = 9;
  r.scenario.objects[1].type = 5;
  r.scenario.objects[1].angular_speed = 1;
  r.scenario.objects[2].id = 9;
  r.scenario.enemies[0].flags = 1;
  r.scenario.enemies[0].rendered_model = triangle_scene();
  r.scenario.items[0].flags = 1;
  r.scenario.items[0].rendered_model = triangle_scene();
  const auto composed = compose_room_runtime(r, 0xffffffff, 0);
  const AmbientTarget targets[] = {
      AmbientTarget::Room,      AmbientTarget::Object, AmbientTarget::Character,
      AmbientTarget::Character, AmbientTarget::Item,   AmbientTarget::Character,
      AmbientTarget::Character};
  const bool clipping[] = {true, false, true, true, true, false, false};
  ck(composed.meshes.size() == 7,
     "Original drawing groups must all be represented");
  for (size_t i = 0; i < 7; ++i)
    ck(composed.meshes[i].ambient_target == targets[i] &&
           composed.meshes[i].model_clipping == clipping[i],
       "Composition must retain source ambient categories and draw order");
  r.scenario.objects[2].model_flags = 8;
  ck(compose_room_runtime(r, 0xffffffff, 0).meshes[1].model_clipping,
     "Source mdflg8 enables clipping for the first normal object");
  r.scenario.objects[2].model_flags = 0;
  near(pixel(compose_room_runtime(r, 0x20, 0)), {100, 0, 0});
  near(pixel(compose_room_runtime(r, 1, 0)), {0, 200, 0});
  near(pixel(compose_room_runtime(r, 8, 0)), {100, 200, 0});
  r.scenario.objects.resize(1);
  r.scenario.objects[0].id = 9;
  r.scenario.objects[0].type = 0;
  near(pixel(compose_room_runtime(r, 4, 0)), {0, 0, 250});
}
void source_model_clipping(bool final_only = false) {
  auto model_scene = [](Vec3 center, float radius, bool material_only = false) {
    auto data = material_model(18, true);
    real(data, 88, center.x);
    real(data, 92, center.y);
    real(data, 96, center.z);
    real(data, 100, radius);
    const Vec3 positions[] = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}};
    for (size_t i = 0; i < 3; ++i) {
      real(data, 176 + i * 32, center.x + positions[i].x);
      real(data, 180 + i * 32, center.y + positions[i].y);
      real(data, 184 + i * 32, center.z);
    }
    if (material_only) {
      data[296] = 255; // Ambient-only command followed immediately by END.
      data[297] = 0;
    }
    Scene scene;
    scene.meshes = decode_model(data, 0);
    for (auto &mesh : scene.meshes)
      for (auto &triangle : mesh.triangles)
        triangle.texture = 0xffff;
    return scene;
  };
  auto compose = [](Scene source, bool character = false) {
    auto visible = triangle_scene();
    visible.textures[0].rgba = {255, 255, 255, 255};
    RoomRuntime runtime;
    runtime.scenario.ambient.emplace();
    runtime.scenario.ambient->colors[0] = {1, 1, 1};
    runtime.scene = character ? visible : source;
    runtime.player_scene = character ? source : visible;
    return compose_room_runtime(runtime, 0x21, 0);
  };
  for (const auto center : std::array<Vec3, 6>{{{100, 0, -3},
                                                {-100, 0, -3},
                                                {0, 100, -3},
                                                {0, -100, -3},
                                                {0, 0, 3},
                                                {0, 0, -21000}}}) {
    auto scene = compose(model_scene(center, .25f, final_only));
    AmbientMaterial retained{255, 255, 255};
    near(pixel(scene, &retained), {255, 255, 255});
    ck(retained == AmbientMaterial{255, 255, 255},
       "Source-culled room models must not consume any ambient material");
    auto trailing = compose(model_scene(center, .25f, true));
    near(pixel(trailing, &retained), {255, 255, 255});
    ck(retained == AmbientMaterial{255, 255, 255},
       "Source model clipping must also gate material-only polygon lists");
  }
  for (float disabled_radius : {0.f, -1.f}) {
    auto scene = compose(model_scene({100, 0, -3}, disabled_radius));
    AmbientMaterial retained{255, 255, 255};
    near(pixel(scene, &retained), {128, 64, 32});
    ck(retained == AmbientMaterial{128, 64, 32},
       "Nonpositive source sphere radius disables model clipping");
  }
  // Strict source comparisons keep spheres exactly touching near/far bounds.
  for (const auto bound : std::array<ModelBounds, 2>{
           ModelBounds{{0, 0, 0}, 1}, ModelBounds{{0, 0, -20000.25f}, .25f}})
    near(pixel(compose(model_scene(bound.center, bound.radius))),
         {128, 64, 32});
  auto translated = material_model(18);
  put(translated, 16, 6); // Translation enabled, rotation/scale disabled.
  real(translated, 24, 100);
  real(translated, 96, -3);
  real(translated, 100, .25f);
  auto mesh = decode_model(translated, 0).at(0);
  ck(mesh.bounds && mesh.bounds->center.x == 100 &&
         mesh.bounds->center.z == -3 && mesh.bounds->radius == .25f,
     "Disk hierarchy must transform model center and preserve source radius");
  const std::vector<ModelMatrix> matrices{
      {2, 0, 0, 200, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1}};
  mesh = decode_model(translated, 0, false, &matrices).at(0);
  ck(mesh.bounds && mesh.bounds->center.x == 200 &&
         mesh.bounds->center.z == -12 && mesh.bounds->radius == .25f,
     "Native posed matrix overrides center transform without scaling radius");
  // The source sphere decides material consumption, independently of pixels.
  auto offscreen = model_scene({0, 0, -3}, .25f);
  for (auto &corner : offscreen.meshes[0].triangles[0].corners)
    corner.position.x += 100;
  near(pixel(compose(offscreen)), {128, 64, 32});
  auto transparent = model_scene({0, 0, -3}, .25f);
  transparent.meshes[0].triangles[0].color[3] = 0;
  near(pixel(compose(transparent)), {128, 64, 32});
  auto occluded = model_scene({0, 0, -4}, .25f);
  AmbientMaterial retained{255, 255, 255};
  // The visible room is drawn first, then an occluded character model.
  near(pixel(compose(occluded, true), &retained), {255, 255, 255});
  ck(retained == AmbientMaterial{128, 64, 32},
     "Occluded source-visible models must still consume material commands");
  pixel(compose(model_scene({100, 0, -3}, .25f), true), &retained);
  ck(retained == AmbientMaterial{128, 64, 32},
     "Main character drawing disables Ninja model-sphere clipping");
  for (size_t p : {88u, 92u, 96u, 100u}) {
    auto invalid = material_model(18);
    real(invalid, p, std::numeric_limits<float>::quiet_NaN());
    bad([&] { decode_model(invalid, 0); });
  }
}
int main(int argc, char **argv) {
  try {
    const std::string mode = argc == 2 ? argv[1] : "commands";
    if (mode == "decoding")
      decoding();
    else if (mode == "rendering")
      rendering();
    else if (mode == "materials")
      materials_and_categories();
    else if (mode == "clipping")
      source_model_clipping();
    else if (mode == "clipping-final")
      source_model_clipping(true);
    else
      commands();
    std::cout << "Room ambient " << mode << " passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
