#include "effects.hpp"
#include "render.hpp"
#include "runtime.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#if __has_include("light.hpp")
#include "light.hpp"
#define NATIVE_LIGHT_API 1
#include "light_color_reference_fixture.hpp"
#include "light_control_reference_fixture.hpp"
#endif
using namespace ashford;
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void bad(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Malformed consumed light state must reject");
}
void put(Bytes &b, size_t p, uint32_t u) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(u >> (8 * i));
}
void real(Bytes &b, size_t p, float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  put(b, p, u);
}
void commands() {
  ScenarioState s;
#ifdef NATIVE_LIGHT_API
  s.lights.emplace();
  s.lights->normal.resize(5);
  s.lights->events.resize(2);
  s.lights->normal[4].flags = 0xf3;
#endif
  run_scenario(Bytes{53, 1, 4, 0, 55, 7, 0}, s);
  ck(s.room_case == 7,
     "Original53 consumes four bytes before next instruction");
#ifdef NATIVE_LIGHT_API
  ck(s.lights->normal[4].flags == 0xf2,
     "Original cemetery53 clears only enable bit");
  run_scenario(Bytes{53, 0, 4, 0, 53, 0, 1, 255, 0}, s);
  ck(s.lights->normal[4].flags == 0xf3 && s.lights->events[1].flags == 1,
     "Source mode0 sets bit1 and any nonzero table selects event lights");
  for (const Bytes &code : {Bytes{53}, Bytes{53, 0, 4}, Bytes{53, 0, 5, 0, 0},
                            Bytes{53, 0, 2, 1, 0}}) {
    auto flags = s.lights->normal[4].flags;
    bad([&] { run_scenario(code, s); });
    ck(s.lights->normal[4].flags == flags && s.room_case == 7,
       "Bad53 is transactional");
  }
  s.lights.reset();
  bad([&] { run_scenario(Bytes{53, 0, 0, 0, 0}, s); });
#endif
}
#ifdef NATIVE_LIGHT_API
Bytes light_fixture() {
  Bytes b(3100);
  real(b, 0, 10);
  put(b, 16, 128);
  put(b, 132, 1132);
  put(b, 128 + 132, 5);
  put(b, 128 + 60, 2252);
  put(b, 128 + 180, 2);
  for (size_t p : {size_t(1132), size_t(2252)}) {
    put(b, p, 0x123);
    put(b, p + 4, 5);
    put(b, p + 8, 7);
    put(b, p + 12, 2);
    put(b, p + 16, 4);
    put(b, p + 20, 3);
    put(b, p + 24, 4);
    real(b, p + 28, 1.25f);
    real(b, p + 40, -2.5f);
    real(b, p + 52, .5f);
    real(b, p + 76, 4);
    real(b, p + 80, 3);
    real(b, p + 84, 1);
    real(b, p + 88, 400);
    real(b, p + 92, 400);
    put(b, p + 116, 3);
    put(b, p + 120, 0x80000001);
    real(b, p + 136, 25);
    real(b, p + 148, -.25f);
    real(b, p + 172, 2);
    real(b, p + 184, 8);
    real(b, p + 188, 9);
    put(b, p + 204, 65535);
    put(b, p + 212, 0xffffffff);
    put(b, p + 216, 0xffffffff);
    put(b, p + 220, 0xffffffff);
  }
  return b;
}
void layout() {
  auto b = light_fixture();
  auto l = decode_room_lights(b);
  ck(l.normal.size() == 5 && l.events.size() == 2,
     "ROM normal/event counts and stride224");
  const auto &v = l.normal[0];
  ck(v.flags == 0x123 && v.type == 5 && v.angular_speed == 7 && v.link == 2 &&
         v.link_index == 4 && v.link_joint == 3 && v.source == 4 &&
         v.position.x == 1.25f && v.local.x == -2.5f && v.vector.x == .5f &&
         v.color == std::array<float, 3>{4, 3, 1} && v.near_range == 400 &&
         v.far_range == 400 && v.mode == 3 &&
         v.counters[0] == int32_t(0x80000001u) && v.work_position.x == 25 &&
         v.work_vector.x == -.25f && v.work_color[0] == 2 && v.work_near == 8 &&
         v.work_far == 9 && v.work_angles[3] == 65535,
     "Explicit disk fields and signed caches survive");
  initialize_room_lights(l);
  ck(l.normal[0].flags == 0x121 && l.normal[0].mode == 0 &&
         l.normal[0].counters[0] == 0 && l.events[0].mode == 3,
     "Init resets only normal work and reserved bit2");
  CameraDefinition cut;
  cut.light_hidden[0] = 0x08000000;
  l.normal[4].flags = 1;
  select_light_cut(l, cut);
  ck(l.normal[4].flags == 1,
     "Original light hide mask uses absolute normal index");
  cut.light_hidden[0] = 0;
  select_light_cut(l, cut);
  ck(l.normal[4].flags == 3, "Visible cut restores normal bit2");
  for (float f : {std::numeric_limits<float>::quiet_NaN(),
                  std::numeric_limits<float>::infinity()}) {
    auto corrupt = b;
    real(corrupt, 1132 + 80, f);
    bad([&] { decode_room_lights(corrupt); });
  }
  auto corrupt = b;
  put(corrupt, 132, 0xfffffff0);
  bad([&] { decode_room_lights(corrupt); });
  corrupt = b;
  put(corrupt, 128 + 132, 257);
  bad([&] { decode_room_lights(corrupt); });
  corrupt = b;
  corrupt.resize(2400);
  bad([&] { decode_room_lights(corrupt); });
}
LightWork directional(std::array<float, 3> color = {1, 1, 1}) {
  LightWork l;
  l.flags = 3;
  l.source = 2;
  l.vector = {0, 0, -1};
  l.color = color;
  return l;
}
LightWork point(float x = 0) {
  LightWork l;
  l.flags = 3;
  l.source = 4;
  l.position = {x, 0, 0};
  l.color = {1, 1, 1};
  l.near_range = 1;
  l.far_range = 10;
  return l;
}
void selection() {
  ScenarioState s;
  s.flags[12][0] = 0x80;
  Camera c;
  c.perspective = 16384;
  CameraDefinition d;
  RoomLights l;
  l.normal.resize(4);
  l.normal.push_back(directional({.5f, .25f, .75f}));
  l.normal.push_back(directional({4, 4, 4}));
  for (int i = 0; i < 5; ++i)
    l.normal.push_back(point(float(i)));
  l.events.push_back(directional({3, 2, 1}));
  auto a = control_lights(l, s, nullptr, c, d, 0);
  ck(a.has_direction &&
         a.direction.color == std::array<float, 3>{.5f, .25f, .75f} &&
         a.point_count == 3 && a.points[2].position.x == 2 &&
         (s.flags[10][0] & 0x10000),
     "First direction and first three points win in table order");
  a = control_lights(l, s, nullptr, c, d, 2);
  ck(a.direction.color == std::array<float, 3>{3, 2, 1} && a.point_count == 0 &&
         (s.flags[10][0] & 0x1000000),
     "Event camera selects event table plus first four reserved normal lights");
  l.normal[0] = point(7);
  a = control_lights(l, s, nullptr, c, d, 2);
  ck(a.point_count == 1 && a.points[0].position.x == 7,
     "Reserved normal lights precede event table");
  l.normal[0].flags = 0;
  l.normal[4].flags = 0;
  l.normal[5].flags = 0;
  d.flags = 0x10;
  d.light_clip = 2;
  l.normal[6].position = {0, 0, -10};
  l.normal[7].position = {10, 0, -10};
  l.normal[8].position = {2, 0, -10};
  l.normal[9].flags = 0;
  l.normal[10].flags = 0;
  a = control_lights(l, s, nullptr, c, d, 0);
  ck(!a.has_direction && a.point_count == 2 && a.points[1].position.x == 2 &&
         !(s.flags[10][0] & 0x1000000),
     "Source projection rejects off-screen point before slot allocation");
}
void dynamics() {
  ScenarioState s;
  s.flags[12][0] = 0x80;
  Camera c;
  CameraDefinition d;
  RoomLights l;
  l.normal.push_back(directional({2, 4, 6}));
  l.normal[0].type = 2;
  l.normal[0].angular_speed = 4;
  auto a = control_lights(l, s, nullptr, c, d, 0);
  ck(a.direction.color == std::array<float, 3>{1, 2, 3} &&
         l.normal[0].counters[0] == 1024,
     "Source sine modulation advances fixed tick phase");
  s.flags[12][0] = 0;
  auto old = l.normal[0].counters[0];
  a = control_lights(l, s, nullptr, c, d, 0);
  ck(a.direction.color == std::array<float, 3>{1, 2, 3} &&
         l.normal[0].counters[0] == old,
     "SP80 off freezes dynamic work and consumes cached parameters");
  s.flags[12][0] = 0x80;
  l.normal[0].type = 5;
  l.normal[0].counters[0] = 0;
  const auto seed = s.random_seed;
  ScenarioState rng;
  rng.random_seed = seed;
  const auto number = scenario_random(rng);
  a = control_lights(l, s, nullptr, c, d, 0);
  const float ratio = -int32_t(number) / -2147483600.f;
  ck(s.random_seed == rng.random_seed &&
         l.normal[0].counters[0] == (uint32_t(1024.f * ratio) & 0x7fff),
     "Randomized flicker consumes shared source RNG exactly once");
  l.normal[0].type = 100;
  l.normal[0].mode = 0;
  control_lights(l, s, nullptr, c, d, 0);
  ck(l.normal[0].mode == 1 && l.normal[0].flags == 3,
     "Flash birth retains this tick");
  control_lights(l, s, nullptr, c, d, 0);
  ck(l.normal[0].flags == 1, "Flash next tick clears source visibility bit2");
  for (int type : {8, 9}) {
    RoomLights orbit;
    orbit.normal.push_back(directional());
    orbit.normal[0].type = type;
    orbit.normal[0].angular_speed = 180;
    const auto before_seed = s.random_seed;
    const auto quarter = control_lights(orbit, s, nullptr, c, d, 0);
    ck(orbit.normal[0].work_angles[3] == (type == 8 ? -16384 : 16384) &&
           std::abs(quarter.direction.vector.x - (type == 8 ? 1.f : -1.f)) <
               .00001f &&
           std::abs(quarter.direction.vector.y) < .00001f &&
           std::abs(quarter.direction.vector.z) < .00001f &&
           s.random_seed == before_seed,
       "Original orbit types rotate in opposite directions without consuming "
       "RNG");
  }
}
void links() {
  ScenarioState s;
  s.flags[12][0] = 0x80;
  s.enemies.resize(3);
  s.enemy_slots = {2};
  auto &enemy = s.enemies[2];
  enemy.flags = 1;
  ModelMatrix matrix{1, 0, 0, 12, 0, 1, 0, 13, 0, 0, 1, 14, 0, 0, 0, 1};
  enemy.model_matrix = matrix;
  enemy.model_work.emplace();
  enemy.model_work->joints.resize(2);
  enemy.model_work->joints[1].matrix = matrix;
  enemy.model_work->joints[1].matrix[3] = 22;
  RoomLights l;
  l.normal.push_back(point());
  l.normal[0].link = 2;
  l.normal[0].local = {1, 2, 3};
  Camera c;
  CameraDefinition d;
  auto a = control_lights(l, s, nullptr, c, d, 0);
  ck(a.points[0].position.x == 13 && a.points[0].position.y == 15 &&
         a.points[0].position.z == 17,
     "Authored enemy links resolve allocated slot and entity matrix");
  l.normal[0].link_joint = 1;
  a = control_lights(l, s, nullptr, c, d, 0);
  ck(a.points[0].position.x == 23,
     "Nonzero linked joint consumes original cached joint index");
  enemy.flags = 0;
  a = control_lights(l, s, nullptr, c, d, 0);
  ck(a.point_count == 0 && l.normal[0].flags == 0,
     "Inactive linked work clears both light bits");
  l.normal[0].flags = 3;
  enemy.flags = 1;
  l.normal[0].link_index = 9;
  bad([&] { control_lights(l, s, nullptr, c, d, 0); });
  l.normal[0].link_index = 0;
  l.normal[0].flags = 3;
  enemy.flags = 0;
  enemy.model_work.reset();
  enemy.pose_owner = 99;
  a = control_lights(l, s, nullptr, c, d, 0);
  ck(l.normal[0].flags == 0 && a.point_count == 0,
     "Inactive linked work does not consume stale model owner caches");
  ScenarioEntity player;
  player.flags = 1;
  player.model_matrix = matrix;
  for (uint32_t link : {1u, 3u, 4u, 5u}) {
    if (link == 3)
      s.objects = {player};
    if (link == 4)
      s.items = {player};
    if (link == 5) {
      auto effects = std::make_shared<NativeEffectPool>();
      effects->works[7].flags = 1;
      effects->works[7].model_matrix = matrix;
      s.effect_pool = effects;
      s.effect_slots = {7};
    }
    l.normal[0].flags = 3;
    l.normal[0].link = link;
    l.normal[0].link_joint = 0;
    a = control_lights(l, s, &player, c, d, 0);
    ck(a.point_count == 1 && a.points[0].position.x == 13,
       "Player/object/item/effect light links consume correct work matrix");
  }
  s.flags[12][0] = 0;
  l.normal[0].link = 1;
  l.normal[0].type = 5;
  l.normal[0].counters[0] = 777;
  l.normal[0].work_color = {.2f, .4f, .6f};
  l.normal[0].work_near = 2;
  l.normal[0].work_far = 7;
  player.model_matrix->at(3) = 32;
  const auto cached_seed = s.random_seed;
  a = control_lights(l, s, &player, c, d, 0);
  ck(a.points[0].position.x == 33 && a.points[0].color[0] == .2f &&
         a.points[0].near_range == 2 && a.points[0].far_range == 7 &&
         l.normal[0].counters[0] == 777 && s.random_seed == cached_seed,
     "Cached dynamic work stays frozen while links follow current model "
     "matrix");
  s.flags[12][0] = 0x80;
  RoomLights rejected;
  rejected.normal.push_back(directional());
  rejected.normal[0].type = 5;
  rejected.normal[0].angular_speed = 4;
  rejected.normal.push_back(point());
  rejected.normal[1].link = 2;
  rejected.normal[1].link_index = 99;
  s.flags[10][0] |= 0x01010000;
  const auto before_flags = s.flags[10][0], before_seed = s.random_seed;
  bad([&] { control_lights(rejected, s, &player, c, d, 0); });
  ck(rejected.normal[0].counters[0] == 0 && s.random_seed == before_seed &&
         s.flags[10][0] == before_flags,
     "Invalid light frame cannot publish partial counters, RNG or flags");
}
uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
float from_bits(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
void reference() {
  int previous = -1;
  ScenarioState s;
  RoomLights work;
  Camera camera;
  CameraDefinition definition;
  for (const auto &entry : light_reference::control) {
    if (entry.type != previous) {
      previous = entry.type;
      s = ScenarioState{};
      s.flags[12][0] = 0x80;
      s.thunder = 8;
      work = RoomLights{};
      work.normal.push_back(directional({2, 4, 6}));
      work.normal[0].type = entry.type;
      work.normal[0].angular_speed = 7;
      if (entry.type == 13 || entry.type == 101)
        work.normal[0].counters[0] = 15000;
    }
    const auto light = control_lights(work, s, nullptr, camera, definition, 0);
    const auto &l = work.normal[0];
    if (l.counters[0] != entry.counter || l.mode != entry.mode ||
        l.flags != entry.flags || s.random_seed != entry.seed)
      throw std::runtime_error("Original dynamic source work mismatch type=" +
                               std::to_string(entry.type) +
                               " tick=" + std::to_string(entry.tick));
    for (size_t c = 0; c < 3; ++c)
      if (bits(light.direction.color[c]) != entry.rgb[c] ||
          bits(l.accumulators[c]) != entry.accumulator[c])
        throw std::runtime_error(
            "Original dynamic source float mismatch type=" +
            std::to_string(entry.type) + " tick=" + std::to_string(entry.tick));
  }
  for (const auto &entry : light_color_reference::cases) {
    NativeLighting l;
    l.has_direction = true;
    l.direction.vector = {0, 0, -1};
    l.single_vector = {0, 0, 1};
    l.single_intensity = 1;
    l.single_ambient = .35f;
    for (size_t c = 0; c < 3; ++c)
      l.direction.color[c] = l.single_color[c] = from_bits(entry.intensity[c]);
    LightMaterial m;
    m.diffuse = {64, 128, 192, 128};
    m.specular = {128, 192, 64};
    const auto color = light_vertex_rgba(l, camera, {0, 0, -3}, {0, 0, 1}, m,
                                         {.35f, .35f, .35f}, entry.mode,
                                         entry.flags, entry.textured);
    for (size_t c = 0; c < 4; ++c)
      if (bits(color[c]) != entry.rgba[c])
        throw std::runtime_error("Original vertex color float mismatch mode=" +
                                 std::to_string(entry.mode) +
                                 " flags=" + std::to_string(entry.flags) +
                                 " channel=" + std::to_string(c));
  }
}
void half(Bytes &b, size_t p, uint16_t u) {
  b.at(p) = uint8_t(u);
  b.at(p + 1) = uint8_t(u >> 8);
}
Bytes material_model(const Bytes &polygon) {
  Bytes b(288 + polygon.size());
  b[0] = 'M';
  b[1] = 'D';
  b[2] = 'L';
  b[3] = 128;
  half(b, 4, 16);
  half(b, 6, 1);
  put(b, 8, 0xffffffff);
  put(b, 16, 6);
  put(b, 20, 64);
  put(b, 60, 0xffffffff);
  put(b, 64, 0xffffffff);
  put(b, 80, 96);
  put(b, 84, 272);
  b[112] = 51;
  half(b, 114, 39);
  half(b, 118, 3);
  for (size_t i = 0; i < 3; ++i) {
    real(b, 176 + i * 32, float(i));
    real(b, 200 + i * 32, 1);
  }
  b[272] = 255;
  std::copy(polygon.begin(), polygon.end(), b.begin() + 288);
  return b;
}
Bytes material_strip() {
  return {64, 0, 5, 0, 1, 0, 3, 0, 0, 0, 1, 0, 2, 0, 255, 0};
}
void materials() {
  ck(LightMaterial{}.exponent == 17,
     "Original retained specular exponent starts at17");
  for (uint8_t type = 17; type <= 23; ++type) {
    Bytes body;
    if (type & 1)
      body.insert(body.end(), {10, 20, 30, 128});
    if (type & 2)
      body.insert(body.end(), {40, 50, 60, 0});
    if (type >= 20)
      body.insert(body.end(), {70, 80, 90, 11});
    Bytes polygon{type, 0, uint8_t(body.size() / 2), 0};
    polygon.insert(polygon.end(), body.begin(), body.end());
    const auto strip = material_strip();
    polygon.insert(polygon.end(), strip.begin(), strip.end());
    auto meshes = decode_model(material_model(polygon), 0);
    LightMaterial m;
    apply_light_material(m, meshes[0].triangles[0].light_material);
    ck(m.diffuse == ((type & 1) ? std::array<uint8_t, 4>{30, 20, 10, 128}
                                : std::array<uint8_t, 4>{255, 255, 255, 255}),
       "Material diffuse/alpha retained independently");
    ck(m.ambient == ((type & 2) ? AmbientMaterial{60, 50, 40}
                                : AmbientMaterial{255, 255, 255}),
       "Material ambient component ordering");
    ck(m.specular == ((type >= 20) ? AmbientMaterial{90, 80, 70}
                                   : AmbientMaterial{255, 255, 255}),
       "Material specular component ordering");
    if (type >= 20)
      ck(m.exponent == 11, "Source specular exponent byte retained");
  }
  Bytes polygon{0, 0, 3, 37, 0, 0, 16, 0, 2, 0, 5, 6, 7, 8};
  const auto strip = material_strip();
  polygon.insert(polygon.end(), strip.begin(), strip.end());
  auto meshes = decode_model(material_model(polygon), 0);
  ck(meshes[0].triangles[0].light_material.exponent == 37,
     "Four-byte exponent and default16 short-count payload stay aligned");
  // Commands after the last strip affect the following model/frame.
  polygon = {17, 0, 2, 0, 10, 20, 30, 128, 255, 0};
  meshes = decode_model(material_model(polygon), 0);
  ck(meshes[0].triangles.empty() && meshes[0].ending_light_material.diffuse ==
                                        std::array<uint8_t, 4>{30, 20, 10, 128},
     "Material-only model preserves final retained commands");
  // One four-corner authored strip becomes two triangles, sharing all corners.
  const Bytes four_strip{64, 9, 6, 0, 1, 0, 4, 0,   0,
                         0,  1, 0, 2, 0, 0, 0, 255, 0};
  const auto grouped = decode_model(material_model(four_strip), 0);
  ck(grouped[0].triangles.size() == 2 && grouped[0].triangles[0].source_strip &&
         grouped[0].triangles[0].source_strip->size() == 4 &&
         grouped[0].triangles[0].source_strip ==
             grouped[0].triangles[1].source_strip,
     "Flattened triangles retain their complete immutable authored strip");
  LightMaterial retained;
  Scene scene;
  scene.meshes = meshes;
  scene.lighting.emplace();
  Camera camera;
  render_camera(scene, camera, 64, 48, nullptr, nullptr, &retained);
  ck(retained.diffuse == std::array<uint8_t, 4>{30, 20, 10, 128},
     "Final command-only material reaches renderer session");
  LightMaterial hidden_retained;
  Camera hidden_camera = camera;
  hidden_camera.hidden[0] = 0x80000000u;
  scene.meshes[0].object = 0;
  render_camera(scene, hidden_camera, 64, 48, nullptr, nullptr,
                &hidden_retained);
  ck(hidden_retained.diffuse == LightMaterial{}.diffuse,
     "Hidden model does not submit retained material commands");
  scene.meshes[0].object = 512;
  scene.meshes[0].model_clipping = true;
  scene.meshes[0].bounds = ModelBounds{{0, 0, 3}, 1};
  render_camera(scene, camera, 64, 48, nullptr, nullptr, &hidden_retained);
  ck(hidden_retained.diffuse == LightMaterial{}.diffuse,
     "Culled whole model does not submit retained material commands");
  scene.meshes[0].model_clipping = false;
  Triangle clipped;
  clipped.texture = 0xffff;
  clipped.corners = {Corner{{-1, -1, 3}}, Corner{{1, -1, 3}},
                     Corner{{0, 1, 3}}};
  clipped.light_material.diffuse = std::array<uint8_t, 4>{1, 2, 3, 128};
  scene.meshes[0].triangles = {clipped};
  scene.meshes[0].ending_light_material.diffuse.reset();
  render_camera(scene, camera, 64, 48, nullptr, nullptr, &hidden_retained);
  ck(hidden_retained.diffuse == std::array<uint8_t, 4>{1, 2, 3, 128},
     "Clipped triangles retain already submitted material commands");
  scene.meshes = meshes;
  const auto before = retained;
  scene.meshes[0].triangles.push_back(Triangle{});
  scene.meshes[0].triangles[0].texture = 9;
  bad([&] {
    render_camera(scene, camera, 64, 48, nullptr, nullptr, &retained);
  });
  ck(retained.diffuse == before.diffuse,
     "Invalid frame cannot publish partial material commands");
  ck(light_blend_register(0x25) == 0x44 && light_blend_register(0x21) == 0x48 &&
         light_blend_register(0) == 0,
     "Original alpha lookup survives material headers");
  bad([&] { light_blend_register(0x20); });
}
void shading() {
  NativeLighting l;
  l.has_direction = true;
  l.direction = {{0, 0, -1}, {1, 1, 1}};
  LightMaterial m;
  Camera c;
  auto color =
      light_vertex_color(l, c, {0, 0, -3}, {0, 0, 1}, m, {0, 0, 0}, 1, 0, true);
  ck(color == std::array<uint8_t, 4>{128, 128, 128, 128},
     "Directional authored normal reaches source vertex RGB128");
  color = light_vertex_color(l, c, {0, 0, -3}, {0, 0, -1}, m, {0, 0, 0}, 1, 0,
                             true);
  ck(color[0] == 0, "Source directional diffuse rejects opposite normal");
  l.has_direction = false;
  l.direction.color = {0, 0, 0};
  l.point_count = 1;
  l.points[0] = {{0, 0, 0}, {1, 1, 1}, 2, 4};
  color =
      light_vertex_color(l, c, {0, 0, -1}, {0, 0, 1}, m, {0, 0, 0}, 1, 0, true);
  ck(color[0] == 128,
     "Near point branch delay slot retains unattenuated diffuse");
  color =
      light_vertex_color(l, c, {0, 0, -4}, {0, 0, 1}, m, {0, 0, 0}, 1, 0, true);
  ck(color[0] == 32, "VU point range includes exact far and applies "
                     "nearSquared/distanceSquared");
  color = light_vertex_color(l, c, {0, 0, -4.01f}, {0, 0, 1}, m, {0, 0, 0}, 1,
                             0, true);
  ck(color[0] == 0, "Beyond far point contribution is zero");
  l.points[0].near_range = 4;
  color =
      light_vertex_color(l, c, {0, 0, -4}, {0, 0, 1}, m, {0, 0, 0}, 1, 0, true);
  ck(color[0] == 128,
     "Equal near/far remains valid without division by far-minus-near");
}
Scene triangle_scene() {
  Scene scene;
  scene.ambient.emplace();
  scene.lighting.emplace();
  scene.lighting->has_direction = true;
  scene.lighting->direction = {{0, 0, -1}, {1, 1, 1}};
  scene.textures.push_back({1, 1, {100, 200, 250, 255}});
  Triangle t;
  t.corners = {Corner{{-1, -1, -3}, 0, 0, {0, 0, 1}},
               Corner{{1, -1, -3}, 0, 0, {0, 0, 1}},
               Corner{{0, 1, -3}, 0, 0, {0, 0, 1}}};
  scene.meshes.push_back({512, {t}});
  return scene;
}
std::array<uint8_t, 3> pixel(const Scene &s) {
  Camera c;
  c.perspective = 16384;
  auto img = render_camera(s, c, 64, 48);
  const size_t p = (24 * 64 + 32) * 3;
  return {img.rgb[p], img.rgb[p + 1], img.rgb[p + 2]};
}
void rendering() {
  auto scene = triangle_scene();
  ck(pixel(scene) == std::array<uint8_t, 3>{100, 200, 250},
     "Native diffuse reaches actual textured pixels");
  scene.lighting->direction.color = {.5f, .5f, .5f};
  ck(pixel(scene) == std::array<uint8_t, 3>{50, 100, 125},
     "Native light color changes actual pixels");
  scene.fog = NativeFog{false, 0x151521, {}};
  ck(pixel(scene) == std::array<uint8_t, 3>{49, 99, 124},
     "Source disabled fog still submits F255 and GS /256 blend");
}
void drawing() {
  RoomLights work;
  work.normal.resize(4);
  work.normal[0].flags = 3;
  work.normal[0].type = 100;
  NativeLighting l;
  l.has_direction = true;
  l.direction = {{0, 0, -1}, {2, 3, 4}};
  l.point_count = 1;
  l.points[0] = {{0, 0, 0}, {2, 4, 6}, 1, 10};
  auto half = drawing_lights(work, l, false, true);
  ck(half.direction.color == std::array<float, 3>{1, 1.5f, 2} &&
         half.points[0].color[1] == 2,
     "Original half group changes both direct and point channels");
  auto easy = drawing_lights(work, l, true, true);
  ck(easy.single_intensity == .6f * 3.f && easy.single_ambient == .2f &&
         work.normal[0].mode == 1,
     "Easy half source intensity .6 multiplies flash birth by3");
  easy = drawing_lights(work, l, true, false);
  ck(easy.single_intensity == 1 && work.normal[0].flags == 1,
     "Easy draw calls advance retained source flash state");
}
void runtime() {
  RoomRuntime r;
  r.scene = triangle_scene();
  r.scenario.ambient = r.scene.ambient;
  r.scenario.lights.emplace();
  r.scenario.lights->normal.resize(5);
  r.scenario.lights->normal[4] = directional();
  r.scenario.flags[12][0] = 0x80;
  r.cuts.resize(1);
  r.cuts[0].variants[0].base.perspective = 16384;
  r.world.cut = 0;
  control_room_lights(r);
  auto before = pixel(compose_room_runtime(r, 0x20, 0));
  run_scenario(Bytes{53, 1, 4, 0, 0}, r.scenario);
  control_room_lights(r);
  auto after = pixel(compose_room_runtime(r, 0x20, 0));
  ck(before[0] > after[0] && after == std::array<uint8_t, 3>{0, 0, 0},
     "Original53 changes actual native composed pixels");
}
#endif
int main(int argc, char **argv) {
  try {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "commands")
      commands();
#ifdef NATIVE_LIGHT_API
    else if (mode == "layout")
      layout();
    else if (mode == "selection")
      selection();
    else if (mode == "dynamics")
      dynamics();
    else if (mode == "links")
      links();
    else if (mode == "shading")
      shading();
    else if (mode == "rendering")
      rendering();
    else if (mode == "drawing")
      drawing();
    else if (mode == "runtime")
      runtime();
    else if (mode == "reference")
      reference();
    else if (mode == "materials")
      materials();
#endif
    else
      throw std::runtime_error("Original native lighting consumer unavailable "
                               "for requested behavior");
    std::cout << "PASS " << mode << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
