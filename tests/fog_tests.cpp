#include "fog.hpp"
#include "fog_reference_fixture.hpp"
#include "render.hpp"
#include "runtime.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *why) {
  if (!v)
    throw std::runtime_error(why);
}
template <class F> void bad(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Malformed consumed fog state must reject");
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
void put(Bytes &b, size_t p, uint32_t u) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(u >> (i * 8));
}
void real(Bytes &b, size_t p, float f) { put(b, p, bits(f)); }
Bytes camera_fixture() {
  Bytes b(1812);
  real(b, 0, 10);
  put(b, 16, 128);
  put(b, 128, 1132);
  put(b, 256, 1);
  put(b, 128 + 0x108, 0xabcdef12);
  real(b, 128 + 0x10c, 128);
  real(b, 128 + 0x110, -90);
  for (size_t v = 0; v < 2; ++v) {
    const size_t p = 1132 + 32 + v * 196;
    put(b, p, 8);
    put(b, p + 84, 16384);
    put(b, p + 184, 0x151521 + uint32_t(v));
    real(b, p + 188, v ? -22.f : 0.f);
    real(b, p + 192, v ? -130.f : -100.f);
  }
  return b;
}
Scene triangle_scene(float depth = 50) {
  Scene s;
  s.ambient.emplace();
  s.ambient->colors[0] = {1, 1, 1};
  s.textures.push_back({1, 1, {100, 200, 250, 255}});
  Triangle t;
  t.corners = {Corner{{-depth, -depth, -depth}, 0, 0},
               Corner{{depth, -depth, -depth}, 0, 0},
               Corner{{0, depth, -depth}, 0, 0}};
  s.meshes.push_back({512, {t}});
  return s;
}
AmbientMaterial pixel(const Scene &s, const Camera &c,
                      AmbientMaterial *material = nullptr) {
  const auto image = render_camera(s, c, 64, 48, material);
  const size_t p = (24 * 64 + 32) * 3;
  return {image.rgb[p], image.rgb[p + 1], image.rgb[p + 2]};
}
void layout_and_command() {
  auto b = camera_fixture();
  const auto room = decode_room_fog(b);
  const auto cuts = decode_room_cuts(b);
  ck(room.color == 0xabcdef12 && room.near_z == 128 && room.far_z == -90,
     "Original signed ROM fog fields must survive");
  ck(cuts[0].variants[0].fog.color == 0x151521 &&
         cuts[0].variants[1].fog.near_z == -22 &&
         cuts[0].variants[1].fog.far_z == -130,
     "Both source camera fog variants must decode");
  ck(resolve_camera(cuts[0], 0).fog.has_value(),
     "Authored enabled camera must retain fog");
  b[1132 + 32] = 0;
  ck(!resolve_camera(decode_room_cuts(b)[0], 0).fog,
     "Disabled authored camera must not enable fog");
  for (size_t p :
       {128u + 0x10c, 128u + 0x110, 1132u + 32 + 188, 1132u + 32 + 192}) {
    auto invalid = b;
    real(invalid, p, std::numeric_limits<float>::quiet_NaN());
    bad([&] {
      if (p < 1132)
        decode_room_fog(invalid);
      else
        decode_room_cuts(invalid);
    });
  }
  auto truncated = b;
  truncated.resize(200);
  bad([&] { decode_room_fog(truncated); });
  ScenarioState s;
  s.flags[10][0] = 0x55;
  s.flags[11][0] = 0x22;
  run_scenario(Bytes{76, 255, 21, 21, 33, 0, 55, 7, 0}, s);
  ck(s.fog.override_color == 0xff151521 &&
         s.flags[10][0] == (0x55 | 0x100000) && s.flags[11][0] == 0x32 &&
         s.room_case == 7,
     "Original76 cursor/ARGB/flags must match source");
  run_scenario(Bytes{76, 0, 99, 88, 77, 255, 0}, s);
  ck(s.fog.override_color == 0xff151521 && s.flags[10][0] == 0x55 &&
         s.flags[11][0] == 0x32,
     "Nonzero76 mode retains stored color and clears only override");
  for (const auto &code : {Bytes{76}, Bytes{76, 255, 21, 21, 33}}) {
    const auto flags = s.flags;
    bad([&] { run_scenario(code, s); });
    ck(s.flags == flags && s.fog.override_color == 0xff151521,
       "Malformed76 must not publish partial flags/color");
  }
}
void numerical_reference() {
  for (const auto &row : source_fog_reference) {
    const auto table = generate_fog_table(row.near_z, row.far_z);
    ck(bits(table.density) == row.density,
       "Fog density must match unchanged original float bits");
    uint64_t digest = 14695981039346656037ull;
    for (float f : table.values)
      for (size_t byte = 0; byte < 4; ++byte)
        digest = (digest ^ uint8_t(bits(f) >> (byte * 8))) * 1099511628211ull;
    ck(digest == row.table_hash,
       "All128 native table values must match unchanged original");
    NativeFog fog{true, 0, table};
    for (size_t i = 0; i < 14; ++i)
      ck(bits(fog_power(fog, from_bits(source_fog_inverse_bits[i]))) ==
             row.samples[i],
         "Native inverse-Z lookup must match original float samples");
    ck(fog_power(fog, std::numeric_limits<float>::infinity()) ==
           fog_power(fog, 256),
       "Positive infinite inverse-Z follows original clamp");
    ck(fog_power(fog, -std::numeric_limits<float>::infinity()) ==
           fog.table.values[127],
       "Negative inverse-Z follows source127 branch");
    bad([&] { fog_power(fog, std::numeric_limits<float>::quiet_NaN()); });
    fog.enabled = false;
    ck(fog_power(fog, std::numeric_limits<float>::quiet_NaN()) == 255,
       "Disabled fog does not consume table/inverse-Z");
  }
  for (auto p : std::array<std::array<float, 2>, 5>{
           {{{0, 0}},
            {{-100, -100}},
            {{0, -.5f}},
            {{std::numeric_limits<float>::infinity(), -100}},
            {{0, -std::numeric_limits<float>::infinity()}}}})
    bad([&] { generate_fog_table(p[0], p[1]); });
}
void control() {
  FogWork f;
  uint32_t st = 0x40000000, gm = 0x80, ef = 0;
  select_fog_cut(f, {0xabcdef12, 128, -90}, true, st, gm, ef);
  ck(gm == 0x90 && f.countdown == 0 && !f.active.enabled,
     "Cut selection requests EOR without changing active fog");
  control_fog(f, st, gm);
  ck(gm == 0x80 && st == 0x40000002 && f.active.enabled &&
         f.active.color == 0xcdef12,
     "EOR enables camera fog and preserves unrelated flags");
  f.override_color = 0xff151521;
  st |= 0x100000;
  gm |= 0x10;
  control_fog(f, st, gm);
  ck(f.active.color == 0x151521, "Source override masks alpha before GS color");
  f.change_ticks = 5;
  select_fog_cut(f, {0x112233, -22, -130}, true, st, gm, ef);
  ck(f.countdown == 2,
     "Original enabled cut retains source change-count minus3");
  const auto old_color = f.active.color;
  control_fog(f, st, gm);
  ck(f.countdown == 1 && f.active.color == old_color && (gm & 0x10),
     "EOR waits for countdown");
  st &= ~0x100000;
  control_fog(f, st, gm);
  ck(f.countdown == 0 && !(gm & 0x10) && f.active.color == 0x112233,
     "Normal EOR applies on countdown reaching0");
  ef = 0x90;
  select_fog_cut(f, {0x334455, 0, -116}, true, st, gm, ef);
  ck(f.countdown == 1 && ef == 0x80,
     "Source effect10 consumes one cut delay tick");
  gm |= 0x200000;
  control_fog(f, st, gm);
  ck(!f.active.enabled && (st & 2) && (gm & 0x10) && f.countdown == 1,
     "Force-disable retains original dirty/timer/state flags");
  gm &= ~0x200000;
  control_fog(f, st, gm);
  ck(f.active.enabled && f.active.color == 0x334455,
     "Pending fog reactivates after force-disable clears");
  select_fog_cut(f, {0, 0, 0}, false, st, gm, ef);
  control_fog(f, st, gm);
  ck(!f.active.enabled && !(st & 2) && !(gm & 0x10),
     "Disabled cut must not consume invalid unused table parameters");
  f.camera = {0x778899, 0, -100};
  f.camera_enabled = false;
  f.countdown = 1;
  gm |= 0x50;
  control_fog(f, st, gm);
  ck(f.countdown == 0 && (gm & 0x10) && !f.active.enabled,
     "Eye EOR waits one extra call after nonzero countdown");
  control_fog(f, st, gm);
  ck(f.active.enabled && !(gm & 0x10),
     "Eye source branch enables fog independently of cut8");
  gm &= ~0x40;
  gm |= 0x10;
  f.countdown = 1;
  control_fog(f, st, gm, 2, true);
  ck(f.countdown == 0 && (gm & 0x10),
     "Keyframe EOR also waits until next0 call");
  control_fog(f, st, gm, 2, false);
  ck(!f.active.enabled && !(st & 2) && (gm & 0x10),
     "Keyframe retains dirty flag and disables by keyframe8");
  control_fog(f, st, gm, 2, true);
  ck(f.active.enabled && (st & 2) && (gm & 0x10),
     "Enabled keyframe fog refreshes every dirty EOR");
  bad([&] { control_fog(f, st, gm, 2); });
  f.camera.far_z = 0;
  const auto old_flags = std::array<uint32_t, 2>{st, gm};
  const auto old_table = f.active.table;
  bad([&] { control_fog(f, st, gm, 2, true); });
  ck(st == old_flags[0] && gm == old_flags[1] &&
         f.active.table.values == old_table.values,
     "Failed EOR must preserve work and flags");
  FogParameters room{0xabcdef12, 0, 0};
  f.change_ticks = 6;
  select_fog_eye(f, room, 2, gm);
  ck(room.color == 0 && room.near_z == 0 && room.far_z == -100 &&
         f.camera_enabled && f.countdown == 2 && (gm & 0x10),
     "Original eye binding normalizes invalid ROM range and uses delay minus4");
}
void rendering() {
  const auto camera =
      resolve_camera(decode_room_cuts(camera_fixture()).at(0), 0);
  auto scene = triangle_scene();
  ck(pixel(scene, camera) == AmbientMaterial{60, 109, 140},
     "Authored camera fog must reach actual GS256-mixed pixels");
  scene.fog = NativeFog{};
  ck(pixel(scene, camera) == AmbientMaterial{100, 200, 250},
     "Explicit current disabled fog overrides stale authored camera");
  scene.fog = NativeFog{true, 0xff0000, generate_fog_table(0, -100)};
  ck(pixel(scene, camera) == AmbientMaterial{178, 99, 124},
     "Current native fog color overrides authored camera");
  scene.meshes[0].fog_enabled = false;
  ck(pixel(scene, camera) == AmbientMaterial{100, 200, 250},
     "Source draw suppression disables vertex fog for selected mesh");
  scene.meshes[0].fog_enabled = true;
  const auto image_before = render_camera(scene, camera, 64, 48);
  scene.meshes[0].triangles[0].ambient_material = AmbientMaterial{1, 2, 3};
  scene.fog->table.values[16] = std::numeric_limits<float>::quiet_NaN();
  AmbientMaterial material{255, 255, 255};
  bad([&] { pixel(scene, camera, &material); });
  ck(material == AmbientMaterial{255, 255, 255},
     "Rejected fog must not publish persistent material");
  ck(!image_before.rgb.empty(), "Fog raster image must contain actual pixels");
  auto varying = triangle_scene();
  varying.fog = NativeFog{true, 0, generate_fog_table(0, -100)};
  varying.meshes[0].triangles[0].corners = {
      Corner{{float(-.8 * 25 / 1.174), -15, -25}, 0, 0},
      Corner{{float(.8 * 100 / 1.174), -60, -100}, 0, 0},
      Corner{{0, 60, -100}, 0, 0}};
  ck(pixel(varying, camera) == AmbientMaterial{18, 36, 46},
     "Fog must interpolate quantized source vertex factors in screen space");
  varying.textures[0].rgba[3] = 0;
  ck(pixel(varying, camera) == AmbientMaterial{18, 22, 28},
     "Transparent fogged fragments must retain the raster background");
  RoomRuntime r;
  r.scene = triangle_scene();
  r.player_scene = triangle_scene();
  r.scenario.fog.active = NativeFog{true, 0, generate_fog_table(0, -100)};
  r.scenario.ambient = r.scene.ambient;
  r.scenario.flags[10][0] |= 2;
  r.scenario.objects.resize(1);
  r.scenario.items.resize(1);
  for (auto *e : {&r.scenario.objects[0], &r.scenario.items[0]}) {
    e->flags = 1;
    e->model_flags = 0x10;
    e->rendered_model = triangle_scene();
  }
  const auto composed = compose_room_runtime(r, 0x2d, 0);
  ck(composed.fog && composed.fog->enabled && composed.meshes.size() == 4 &&
         composed.meshes[0].fog_enabled && !composed.meshes[1].fog_enabled &&
         !composed.meshes[2].fog_enabled && composed.meshes[3].fog_enabled,
     "Current scene/normal object/item/player fog policy must reach renderer");
}
void drawing_policy() {
  RoomRuntime r;
  r.scene = triangle_scene();
  r.scenario.ambient = r.scene.ambient;
  r.scenario.fog.active =
      NativeFog{true, 0x151521, generate_fog_table(0, -100)};
  r.scenario.flags[10][0] = 2;
  r.scenario.objects.resize(1);
  auto &object = r.scenario.objects[0];
  object.flags = 1;
  object.id = 1000;
  object.model_flags = 0x10;
  object.rendered_model = triangle_scene();
  Camera camera;
  camera.perspective = 16384;
  ck(pixel(compose_room_runtime(r, 6, 0), camera) ==
         AmbientMaterial{100, 200, 250},
     "First half-light object must suppress fog independently of model "
     "clipping");
  object.id = 9;
  object.type = 5;
  object.angular_speed = 1;
  ck(pixel(compose_room_runtime(r, 4, 0), camera) ==
         AmbientMaterial{60, 109, 140},
     "Special object drawing does not perform normal-object fog suppression");
  r.scenario.objects.insert(r.scenario.objects.begin(), ScenarioEntity{});
  auto &normal = r.scenario.objects[0];
  normal.flags = 1;
  normal.id = 9;
  normal.rendered_model = triangle_scene();
  for (auto &corner : normal.rendered_model->meshes[0].triangles[0].corners)
    corner.position.x += 1000;
  ck(pixel(compose_room_runtime(r, 4, 0), camera) ==
         AmbientMaterial{60, 109, 140},
     "Prior normal-object clipping state must not suppress special-object fog");
  normal.model_flags = 0x10;
  r.scenario.fog.active.enabled = false;
  ck(pixel(compose_room_runtime(r, 4, 0), camera) ==
         AmbientMaterial{60, 109, 140},
     "Normal mdflg10 restore must enable fog for later models even after "
     "force-disable");
  const auto composed = compose_room_runtime(r, 4, 0);
  render_camera(composed, camera, 64, 48, nullptr,
                &r.scenario.fog.active.enabled);
  ck(r.scenario.fog.active.enabled, "Completed drawing must retain restored "
                                    "global fog state for next EOR/frame");
  ck(pixel(compose_room_runtime(r, 0x20, 0), camera) ==
         AmbientMaterial{60, 109, 140},
     "Restored original fog state must reach room geometry on next frame");
  auto invalid = composed;
  invalid.fog->table.values[16] = std::numeric_limits<float>::quiet_NaN();
  bool end = false;
  AmbientMaterial material{255, 255, 255};
  bad([&] { render_camera(invalid, camera, 64, 48, &material, &end); });
  ck(!end && material == AmbientMaterial{255, 255, 255},
     "Rejected draw must not publish ambient or completed fog state");
}
void runtime_control() {
  RoomRuntime r;
  r.scene = triangle_scene();
  r.scenario.ambient = r.scene.ambient;
  r.cuts = decode_room_cuts(camera_fixture());
  r.world.cut = 0;
  control_room_fog(r);
  Camera camera;
  camera.perspective = 16384;
  ck(pixel(compose_room_runtime(r, 0x20, 0), camera) ==
         AmbientMaterial{60, 109, 140},
     "Actual runtime cut selection/EOR must reach composed pixels");
  run_scenario(Bytes{76, 255, 255, 0, 0, 0, 0}, r.scenario);
  control_room_fog(r);
  ck(pixel(compose_room_runtime(r, 0x20, 0), camera) ==
         AmbientMaterial{178, 99, 124},
     "Source76 override must propagate through runtime EOR/composition/pixels");
  run_scenario(Bytes{76, 0, 0, 0, 0, 1, 0}, r.scenario);
  r.scenario.fog.change_ticks = 5;
  auto cut = r.cuts[0];
  cut.variants[0].fog = {0x0000ff, 0, -100};
  r.cuts.push_back(cut);
  r.world.cut = 1;
  control_room_fog(r);
  ck(r.scenario.fog.countdown == 1 &&
         pixel(compose_room_runtime(r, 0x20, 0), camera) ==
             AmbientMaterial{178, 99, 124},
     "Cut transition must preserve active source fog during delay");
  control_room_fog(r);
  ck(pixel(compose_room_runtime(r, 0x20, 0), camera) ==
         AmbientMaterial{49, 99, 252},
     "New cut fog becomes visible exactly when normal EOR countdown reaches0");
  cut.variants[0].fog.far_z = 0;
  r.cuts.push_back(cut);
  r.world.cut = 2;
  r.scenario.fog.change_ticks = 3;
  const auto old = r.scenario.fog;
  const auto flags = r.scenario.flags;
  const auto old_cut = r.fog_cut;
  bad([&] { control_room_fog(r); });
  ck(r.scenario.fog.camera.far_z == old.camera.far_z &&
         r.scenario.fog.active.table.values == old.active.table.values &&
         r.scenario.flags == flags && r.fog_cut == old_cut,
     "Failed current cut fog must preserve camera work/flags/cached cut");
  r.room_fog = {0xabcdef, 0, 0};
  r.scenario.flags[11][0] |= 0x40;
  control_room_fog(r);
  ck(r.scenario.fog.active.enabled && r.scenario.fog.camera.near_z == 0 &&
         r.scenario.fog.camera.far_z == -100 &&
         r.scenario.fog.active.color == 0,
     "Actual eye fog uses normalized room parameters without reading invalid "
     "cut");
}
void dynamic_binding() {
  size_t failures = 0;
  for (uint16_t mask : {0x40, 0x80, 0xc0})
    for (size_t binding = 0; binding < 3; ++binding) {
      RoomRuntime r;
      r.cuts = decode_room_cuts(camera_fixture());
      r.world.cut = 0;
      if (binding)
        control_room_fog(r);
      if (binding == 1) {
        r.cuts.push_back(r.cuts[0]);
        r.world.cut = 1;
      }
      r.cuts[*r.world.cut].variants[0].flags |= mask;
      r.scenario.effect_flags |= 0x10;
      r.scenario.fog.change_ticks = 5;
      const auto old = r.scenario.fog;
      const auto flags = r.scenario.flags;
      const auto effects = r.scenario.effect_flags;
      const auto cut = r.fog_cut;
      const bool bound = r.fog_bound, eye = r.fog_eye;
      const auto room = r.room_fog;
      bool rejected = false;
      try {
        control_room_fog(r);
      } catch (const std::runtime_error &error) {
        rejected =
            std::string(error.what()).find("dynamic") != std::string::npos;
      }
      if (!rejected) {
        std::cerr << "Accepted unsupported dynamic fog mask=" << mask
                  << " binding=" << binding << '\n';
        ++failures;
        continue;
      }
      ck(r.scenario.fog.override_color == old.override_color &&
             r.scenario.fog.countdown == old.countdown &&
             r.scenario.fog.change_ticks == old.change_ticks &&
             r.scenario.fog.camera.color == old.camera.color &&
             r.scenario.fog.camera.near_z == old.camera.near_z &&
             r.scenario.fog.camera.far_z == old.camera.far_z &&
             r.scenario.fog.camera_enabled == old.camera_enabled &&
             r.scenario.fog.active.enabled == old.active.enabled &&
             r.scenario.fog.active.color == old.active.color &&
             r.scenario.fog.active.table.density == old.active.table.density &&
             r.scenario.fog.active.table.values == old.active.table.values &&
             r.scenario.flags == flags && r.scenario.effect_flags == effects &&
             r.fog_cut == cut && r.fog_bound == bound && r.fog_eye == eye &&
             r.room_fog.color == room.color &&
             r.room_fog.near_z == room.near_z && r.room_fog.far_z == room.far_z,
         "Rejected dynamic binding must preserve all fog work/flags/cache");
    }
  ck(failures == 0, "Source dynamic fog must reject for fresh, changed and "
                    "cached cut bindings");
  RoomRuntime eye;
  eye.cuts = decode_room_cuts(camera_fixture());
  eye.cuts[0].variants[0].flags |= 0xc0;
  eye.world.cut = 0;
  eye.room_fog = {0x123456, 0, -100};
  eye.scenario.flags[11][0] |= 0x40;
  control_room_fog(eye);
  ck(eye.scenario.fog.active.enabled &&
         eye.scenario.fog.active.color == 0x123456,
     "Source eye fog must bind ROM data without consuming dynamic cut fields");
}
int main(int argc, char **argv) {
  try {
    const std::string mode = argc == 2 ? argv[1] : "commands";
    if (mode == "reference")
      numerical_reference();
    else if (mode == "control")
      control();
    else if (mode == "rendering")
      rendering();
    else if (mode == "drawing")
      drawing_policy();
    else if (mode == "runtime")
      runtime_control();
    else if (mode == "dynamic")
      dynamic_binding();
    else
      layout_and_command();
    std::cout << "Native fog " << mode << " passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
