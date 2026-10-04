#include "actor.hpp"
#include "normal_reference_fixture.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
using namespace ashford;
namespace {
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void bad(F f, const char *message) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, message);
}
// Allows the same behavioral fixture to run against BASE's old Corner API.
template <class T, class = void> struct NormalReader {
  static std::optional<Vec3> read(const T &) { return {}; }
};
template <class T> struct NormalReader<T, std::void_t<decltype(T{}.normal)>> {
  static std::optional<Vec3> read(const T &corner) { return corner.normal; }
};
Vec3 normal(const Corner &corner) {
  const auto value = NormalReader<Corner>::read(corner);
  ck(value.has_value(), "Authored normal discarded before emitted corner");
  return *value;
}
void equal(Vec3 a, Vec3 b, const char *message) {
  ck(std::abs(a.x - b.x) < .00001f && std::abs(a.y - b.y) < .00001f &&
         std::abs(a.z - b.z) < .00001f,
     message);
}
uint32_t bits(float value) {
  uint32_t out;
  std::memcpy(&out, &value, 4);
  return out;
}
float value(uint32_t word) {
  float out;
  std::memcpy(&out, &word, 4);
  return out;
}
void put(Bytes &b, size_t p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(value >> (i * 8));
}
void half(Bytes &b, size_t p, uint16_t value) {
  b.at(p) = uint8_t(value);
  b.at(p + 1) = uint8_t(value >> 8);
}
void flt(Bytes &b, size_t p, float value) { put(b, p, bits(value)); }
void vector(Bytes &b, size_t p, Vec3 v) {
  flt(b, p, v.x);
  flt(b, p + 4, v.y);
  flt(b, p + 8, v.z);
}
ModelMatrix identity() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
constexpr std::array<Vec3, 3> authored{
    {{1.25f, -2.5f, 3.75f}, {0, 0, 0}, {-.75f, .5f, 2}}};
struct Fixture {
  ActorModel actor;
  size_t vertex = 0, polygon = 0;
};
Fixture fixture(size_t count = 2, bool skin = true) {
  Fixture f;
  const size_t descriptor = (16 + count * 52 + 15) & ~size_t(15);
  f.vertex = descriptor + 32;
  f.polygon = f.vertex + 176;
  auto &b = f.actor.model;
  b.resize(f.polygon + 48);
  b[0] = 'M';
  b[1] = 'D';
  b[2] = 'L';
  b[3] = 128;
  half(b, 4, 16);
  half(b, 6, uint16_t(count));
  put(b, 8, 0xffffffff);
  for (size_t i = 0; i < count; ++i) {
    const size_t o = 16 + i * 52;
    put(b, o, 6);
    put(b, o + 4, i == 0 ? uint32_t(descriptor - 16) : 0xffffffff);
    put(b, o + 44, i + 1 < count ? uint32_t((i + 1) * 52) : 0xffffffff);
    put(b, o + 48, 0xffffffff);
  }
  put(b, descriptor, uint32_t(f.vertex - 16));
  put(b, descriptor + 4, uint32_t(f.polygon - 16));
  b[f.vertex] = 51;
  half(b, f.vertex + 2, 39);
  half(b, f.vertex + 6, 3);
  for (size_t i = 0; i < 3; ++i) {
    vector(b, f.vertex + 64 + i * 32, {float(i), float(i == 1), float(i == 2)});
    vector(b, f.vertex + 80 + i * 32, authored[i]);
    // Neither W lane is a consumed xyz normal or position component.
    put(b, f.vertex + 76 + i * 32, 0x7fc12345);
    put(b, f.vertex + 92 + i * 32, 0x7fc54321);
  }
  b[f.vertex + 160] = 255;
  b[f.polygon] = 64;
  half(b, f.polygon + 2, 5);
  half(b, f.polygon + 4, 1);
  half(b, f.polygon + 6, 3);
  for (size_t i = 0; i < 3; ++i)
    half(b, f.polygon + 8 + i * 2, uint16_t(i));
  b[f.polygon + 14] = 255;
  if (skin) {
    ck(count >= 2, "Skin fixture requires joint");
    auto &s = f.actor.skin;
    s.resize(4 + 24 + (count - 1) * 8 + 4);
    s[0] = 'S';
    s[1] = 'K';
    s[2] = 'I';
    s[3] = 'N';
    put(s, 4, 3);
    put(s, 8, 0);
    put(s, 12, 3);
    for (size_t i = 0; i < 3; ++i)
      put(s, 16 + i * 4, 0x01040000);
    for (size_t i = 1; i < count; ++i) {
      put(s, 28 + (i - 1) * 8, 1);
      put(s, 32 + (i - 1) * 8, 0);
    }
    put(s, s.size() - 4, 0xffffffff);
  }
  return f;
}
void validation() {
  auto f = fixture(1, false);
  flt(f.actor.model, f.vertex + 80, std::numeric_limits<float>::quiet_NaN());
  bad([&] { decode_model(f.actor.model, 0); },
      "Nonfinite authored normal xyz accepted");
  f = fixture(1, false);
  auto m = identity();
  m[0] = std::numeric_limits<float>::max();
  vector(f.actor.model, f.vertex + 80, {2, 0, 0});
  for (size_t i = 0; i < 3; ++i)
    vector(f.actor.model, f.vertex + 64 + i * 32, {});
  const std::vector<ModelMatrix> matrices{m};
  bad([&] { decode_model(f.actor.model, 0, false, &matrices); },
      "Normal transform overflow accepted");
}
void layout() {
  auto f = fixture(1, false);
  auto meshes = decode_model(f.actor.model, 0);
  for (size_t i = 0; i < 3; ++i)
    equal(normal(meshes[0].triangles[0].corners[i]), authored[i],
          "Authored zero/nonunit normal changed");
  auto m = identity();
  m[0] = 2;
  m[1] = .5;
  m[5] = 3;
  m[6] = -.25;
  m[10] = 4;
  m[3] = 100;
  m[7] = -200;
  m[11] = 300;
  const std::vector<ModelMatrix> matrices{m};
  auto transformed = decode_model(f.actor.model, 0, false, &matrices);
  equal(
      normal(transformed[0].triangles[0].corners[0]), {1.25f, -8.4375f, 15},
      "Normal requires source linear matrix, no translation or normalization");
  equal(normal(transformed[0].triangles[0].corners[1]), {},
        "Translated zero normal moved");
  half(f.actor.model, f.polygon + 6, uint16_t(int16_t(-3)));
  auto reversed = decode_model(f.actor.model, 0);
  equal(normal(reversed[0].triangles[0].corners[0]), authored[1],
        "Negative strip normal winding wrong");
  equal(normal(reversed[0].triangles[0].corners[1]), authored[0],
        "Negative strip normal winding wrong");
  half(f.actor.model, f.polygon + 6, 3);
  half(f.actor.model, f.vertex + 4, 41);
  for (size_t i = 0; i < 3; ++i)
    half(f.actor.model, f.polygon + 8 + i * 2, uint16_t(41 + i));
  auto offset = decode_model(f.actor.model, 0);
  equal(normal(offset[0].triangles[0].corners[2]), authored[2],
        "Normal cache offset ignored");
  for (const uint8_t type : {uint8_t(65), uint8_t(66)}) {
    auto uv = fixture(1, false);
    uv.actor.model[uv.polygon] = type;
    half(uv.actor.model, uv.polygon + 2, 11);
    for (size_t i = 0; i < 3; ++i) {
      half(uv.actor.model, uv.polygon + 8 + i * 6, uint16_t(i));
      half(uv.actor.model, uv.polygon + 10 + i * 6,
           uint16_t(i == 1 ? (type == 65 ? 255 : 1023) : 0));
      half(uv.actor.model, uv.polygon + 12 + i * 6,
           uint16_t(i == 2 ? (type == 65 ? 255 : 1023) : 0));
    }
    uv.actor.model[uv.polygon + 26] = 255;
    const auto decoded = decode_model(uv.actor.model, 0);
    equal(normal(decoded[0].triangles[0].corners[2]), authored[2],
          "UV strip lost normal");
    ck(decoded[0].triangles[0].corners[1].u == 1, "Normal broke UV decode");
  }
  auto four = fixture(1, false);
  auto &b = four.actor.model;
  b.insert(b.begin() + four.vertex + 160, 32, 0);
  four.polygon += 32;
  put(b, 84, uint32_t(four.polygon - 16));
  half(b, four.vertex + 2, 47);
  half(b, four.vertex + 6, 4);
  vector(b, four.vertex + 160, {3, 2, 1});
  vector(b, four.vertex + 176, {7, 8, 9});
  half(b, four.polygon + 2, 6);
  half(b, four.polygon + 6, 4);
  half(b, four.polygon + 14, 3);
  b[four.polygon + 16] = 255;
  const auto alternating = decode_model(b, 0);
  ck(alternating[0].triangles.size() == 2,
     "Four-vertex normal strip incomplete");
  equal(normal(alternating[0].triangles[1].corners[0]), authored[2],
        "Alternating strip normal winding wrong");
  equal(normal(alternating[0].triangles[1].corners[1]), authored[1],
        "Alternating strip normal winding wrong");
  equal(normal(alternating[0].triangles[1].corners[2]), {7, 8, 9},
        "Fourth normal record lost");
  auto overwritten = fixture(1, false);
  auto &o = overwritten.actor.model;
  const Bytes chunk(o.begin() + overwritten.vertex,
                    o.begin() + overwritten.vertex + 160);
  o.insert(o.begin() + overwritten.vertex + 160, chunk.begin(), chunk.end());
  overwritten.polygon += 160;
  put(o, 84, uint32_t(overwritten.polygon - 16));
  vector(o, overwritten.vertex + 240, {101, 102, 103});
  const auto refreshed = decode_model(o, 0);
  equal(normal(refreshed[0].triangles[0].corners[0]), {101, 102, 103},
        "Second chunk failed to update normal cache");
}
void skin() {
  const std::array<float, 10> levels{.1f, .2f, .3f, .4f, .5f,
                                     .6f, .7f, .8f, .9f, 1};
  for (size_t level = 0; level < 10; ++level) {
    auto f = fixture();
    for (size_t i = 0; i < 3; ++i)
      put(f.actor.skin, 16 + i * 4, uint32_t(0x01000000 | (level << 16)));
    auto bindings = decode_skin(f.actor.skin, 2);
    ck(bits(bindings[0].vertices[0].rotation_weight) == bits(levels[level]),
       "Skin level differs from original literal float table");
    auto pose = actor_bind_pose(f.actor);
    pose[1].angles = std::array<int32_t, 3>{0, 0, 16384};
    ActorPlacement placement;
    placement.position = {100, -200, 300};
    auto work = calculate_actor_model_work(f.actor, pose, {}, placement);
    // Use exact linear matrices to isolate source vector blending from trig.
    work.joints[1].matrix = {0, -3, 0, 300, 2, 0, .5, -100,
                             0, 0,  4, 250, 0, 0, 0,  1};
    const auto before = f.actor.model, before_skin = f.actor.skin;
    const auto rendered = pose_actor_model_work(f.actor, pose, work);
    const Vec3 a{3.75f, -3.125f, 15}, b{7.5f, 4.375f, 15};
    const float w = levels[level];
    const Vec3 expected =
        level == 9 ? b
                   : Vec3{a.x + w * (b.x - a.x), a.y + w * (b.y - a.y),
                          a.z + w * (b.z - a.z)};
    equal(normal(rendered.meshes[0].triangles[0].corners[0]), expected,
          "Skinned normal was not blended once using current matrices");
    ck(f.actor.model == before && f.actor.skin == before_skin,
       "Skin changed imported input");
    const auto prepared = prepare_actor_skin(f.actor);
    const auto preconverted = pose_actor_model_work(prepared, pose, work);
    equal(normal(preconverted.meshes[0].triangles[0].corners[0]), expected,
          "Bind conversion incorrectly translated normal");
  }
  auto f = fixture(1, false);
  const auto rigid =
      pose_actor_rotated(f.actor, {}, {500, 600, 700}, {0, 0, 16384});
  equal(normal(rigid.meshes[0].triangles[0].corners[0]), {2.5f, 1.25f, 3.75f},
        "Rigid actor normal failed rotated placement");
}
void levels() {
  for (size_t level = 0; level < 10; ++level) {
    auto f = fixture();
    for (size_t i = 0; i < 3; ++i)
      put(f.actor.skin, 16 + i * 4, uint32_t(0x01000000 | (level << 16)));
    const auto decoded = decode_skin(f.actor.skin, 2);
    ck(bits(decoded[0].vertices[0].rotation_weight) ==
           normal_reference::levels[level],
       "Decoded skin level is not the literal source float");
  }
}
void facial() {
  auto f = fixture(16, false);
  f.actor.skin_layout = SkinLayout::FaceModel;
  f.actor.skin_preconverted = true;
  auto &s = f.actor.skin;
  s = {'S', 'K', 'I', 'N'};
  auto append = [&](uint32_t v) {
    const auto p = s.size();
    s.resize(p + 4);
    put(s, p, v);
  };
  append(3);
  append(0);
  append(3);
  for (size_t i = 0; i < 3; ++i)
    append(0x0b040000); // Source11 -> native15.
  for (size_t i = 1; i < 11; ++i)
    append(0);
  append(1);
  append(0);
  append(0xffffffff);
  // A separate rigid eye stream must not consume skin records11..14.
  auto &b = f.actor.model;
  const auto d = b.size(), v = d + 32, p = v + 176;
  b.resize(p + 48);
  put(b, 16 + 11 * 52 + 4, uint32_t(d - 16));
  put(b, d, uint32_t(v - 16));
  put(b, d + 4, uint32_t(p - 16));
  std::copy_n(b.begin() + f.vertex, 176, b.begin() + v);
  std::copy_n(b.begin() + f.polygon, 48, b.begin() + p);
  auto pose = actor_bind_pose(f.actor);
  pose[15].angles = std::array<int32_t, 3>{0, 0, 16384};
  auto work = calculate_actor_model_work(f.actor, pose);
  work.joints[15].matrix = {0, -1, 0, 100, 1, 0, 0, 200,
                            0, 0,  1, 300, 0, 0, 0, 1};
  work.joints[11].matrix = {5, 0, 0, 1000, 0, 6, 0, 2000,
                            0, 0, 7, 3000, 0, 0, 0, 1};
  const auto before = b;
  const auto scene = pose_actor_model_work(f.actor, pose, work);
  ck(scene.meshes.size() == 2, "Facial normal draw range changed");
  equal(normal(scene.meshes[0].triangles[0].corners[0]),
        {1.875f, -.625f, 3.75f},
        "Facial skin normal ignored remapped source joint");
  equal(normal(scene.meshes[1].triangles[0].corners[0]), {6.25f, -15, 26.25f},
        "Rigid skipped facial eye normal was skinned");
  work.first_joint = 0;
  work.joint_count = 1;
  const auto selected = pose_actor_model_work(f.actor, pose, work);
  ck(selected.meshes.size() == 1, "Selected facial range drew skipped eye");
  equal(normal(selected.meshes[0].triangles[0].corners[0]),
        {1.875f, -.625f, 3.75f},
        "Selected facial range lost whole-model influence");
  ck(b == before, "Facial posing mutated retained model");
}
void placement() {
  auto f = fixture(1, false);
  ActorPlacement linked;
  linked.flags = 0x80;
  linked.linked_offset = {40, 50, 60};
  linked.parent_joint =
      ModelMatrix{2, .5, 0, 100, 0, 3, -.25, 200, 0, 0, 4, 300, 0, 0, 0, 1};
  const auto pose = actor_bind_pose(f.actor);
  auto work = calculate_actor_model_work(f.actor, pose, {}, linked);
  const auto scene = pose_actor_model_work(f.actor, pose, work);
  equal(normal(scene.meshes[0].triangles[0].corners[0]), {1.25f, -8.4375f, 15},
        "Linked matrix or offset changed original linear normal");
  const std::vector<ModelMatrix> matrices{work.joints[0].matrix};
  const auto whole = decode_model(f.actor.model, 0, false, &matrices);
  equal(normal(whole[0].triangles[0].corners[0]),
        normal(scene.meshes[0].triangles[0].corners[0]),
        "Cached and direct normal matrix paths differ");
}
void reference() {
  for (const auto &entry : normal_reference::cases) {
    auto f = fixture(1, false);
    auto m = identity();
    for (size_t row = 0; row < 3; ++row)
      for (size_t column = 0; column < 3; ++column)
        m[row * 4 + column] = value(entry.input[row * 3 + column]);
    m[3] = 100;
    m[7] = 200;
    m[11] = 300;
    vector(f.actor.model, f.vertex + 80,
           {value(entry.input[9]), value(entry.input[10]),
            value(entry.input[11])});
    const std::vector<ModelMatrix> matrices{m};
    const auto decoded = decode_model(f.actor.model, 0, false, &matrices);
    const auto n = normal(decoded[0].triangles[0].corners[0]);
    ck(bits(n.x) == entry.output[0] && bits(n.y) == entry.output[1] &&
           bits(n.z) == entry.output[2],
       "Transformed normal differs from unchanged original scalar equations");
  }
}
void morph() {
  auto source = fixture(), target = source;
  source.actor = prepare_actor_skin(source.actor);
  target.actor = prepare_actor_skin(target.actor);
  vector(target.actor.model, target.vertex + 64, {4, 5, 6});
  vector(target.actor.model, target.vertex + 80, {-1, 2, -3});
  vector(target.actor.model, target.vertex + 112,
         {9, 9, 9}); // Same vertex1 position: ignored normal.
  auto pose = actor_bind_pose(source.actor);
  const auto morphed = morph_actor_model(source.actor, target.actor, pose, 500);
  auto work = calculate_actor_model_work(morphed, pose);
  work.joints[1].matrix = identity();
  work.joints[1].matrix[0] = 2;
  const auto rendered = pose_actor_model_work(morphed, pose, work);
  equal(normal(rendered.meshes[0].triangles[0].corners[0]),
        {.25f, -.25f, .375f}, "Morphed normal did not reach posed mesh");
  equal(normal(rendered.meshes[0].triangles[0].corners[1]), {},
        "Unchanged morph position must preserve source normal");
  auto runtime = RoomRuntime{};
  ScenarioEntity object;
  object.flags = 1;
  object.rendered_model = rendered;
  runtime.scenario.objects.push_back(object);
  const auto composed = compose_room_runtime(runtime, 4, 0);
  equal(normal(composed.meshes[0].triangles[0].corners[0]),
        {.25f, -.25f, .375f}, "Room composition discarded posed normal");
  const auto saved = runtime.scenario.objects[0].rendered_model;
  flt(source.actor.model, source.vertex + 80,
      std::numeric_limits<float>::infinity());
  bad(
      [&] {
        runtime.scenario.objects[0].rendered_model =
            pose_actor_model_work(source.actor, pose, work);
      },
      "Invalid normal published an actor pose");
  equal(normal(runtime.scenario.objects[0]
                   .rendered_model->meshes[0]
                   .triangles[0]
                   .corners[0]),
        normal(saved->meshes[0].triangles[0].corners[0]),
        "Failed pose modified retained scene");
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::string mode = argc > 1 ? argv[1] : "layout";
    if (mode == "layout")
      layout();
    else if (mode == "validation")
      validation();
    else if (mode == "skin")
      skin();
    else if (mode == "morph")
      morph();
    else if (mode == "levels")
      levels();
    else if (mode == "reference")
      reference();
    else if (mode == "facial")
      facial();
    else if (mode == "placement")
      placement();
    else
      throw std::runtime_error("Unknown native normal check");
    std::cout << "Native authored normals " << mode << ": PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
