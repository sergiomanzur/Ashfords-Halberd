#include "actor.hpp"
#include "runtime.hpp"
#include "zombie.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
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
  ck(rejected, "Invalid morph input accepted");
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
void half(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
uint32_t word(const Bytes &b, size_t p) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i)
    v |= uint32_t(b.at(p + i)) << (8 * i);
  return v;
}
void flt(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
float read(const Bytes &b, size_t p) {
  auto v = word(b, p);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}
ActorModel fixture() {
  ActorModel a;
  auto &b = a.model;
  b.resize(400);
  b[0] = 'M';
  b[1] = 'D';
  b[2] = 'L';
  b[3] = 128;
  half(b, 4, 16);
  half(b, 6, 2);
  put(b, 8, 0xffffffff);
  put(b, 16, 6);
  put(b, 20, 128);
  flt(b, 24, 10);
  put(b, 60, 52);
  put(b, 64, 0xffffffff);
  put(b, 68, 6);
  put(b, 72, 0xffffffff);
  flt(b, 80, 2);
  put(b, 112, 0xffffffff);
  put(b, 116, 0xffffffff);
  put(b, 144, 152);
  put(b, 148, 328);
  b[168] = 51;
  half(b, 170, 39);
  half(b, 174, 3);
  flt(b, 232, 1);
  flt(b, 268, 1);
  flt(b, 248, 2);
  flt(b, 280, 3);
  flt(b, 312, 4);
  put(b, 244, 0x12345678);
  put(b, 260, 0x87654321);
  b[328] = 255;
  b[344] = 64;
  half(b, 346, 5);
  half(b, 348, 1);
  half(b, 350, 3);
  for (size_t i = 0; i < 3; ++i)
    half(b, 352 + i * 2, uint16_t(i));
  b[358] = 255;
  a.skin.resize(40);
  auto &s = a.skin;
  s[0] = 'S';
  s[1] = 'K';
  s[2] = 'I';
  s[3] = 'N';
  put(s, 4, 3);
  put(s, 8, 0);
  put(s, 12, 3);
  for (size_t i = 0; i < 3; ++i)
    put(s, 16 + i * 4, 0x01090000);
  put(s, 28, 1);
  put(s, 32, 0);
  put(s, 36, 0xffffffff);
  return a;
}
int main() {
  try {
    const auto raw = fixture();
    auto converted = prepare_actor_skin(raw);
    ck(converted.skin_preconverted && read(converted.model, 236) == -2 &&
           (word(converted.model, 16) & 0x80000000),
       "Source bind conversion failed");
    auto pose = actor_bind_pose(raw);
    pose[1].position->y = 8;
    ActorPlacement placement;
    placement.position = {100, 200, 300};
    auto cache = calculate_actor_model_work(raw, pose, {}, placement);
    const auto original = pose_actor_model_work(raw, pose, cache),
               prepared = pose_actor_model_work(converted, pose, cache);
    for (size_t i = 0; i < 3; ++i) {
      auto p = original.meshes[0].triangles[0].corners[i].position,
           q = prepared.meshes[0].triangles[0].corners[i].position;
      ck(p.x == q.x && p.y == q.y && p.z == q.z,
         "Prepared skin changed baseline drawing");
    }
    auto target = raw;
    target.skin.clear();
    auto source = raw;
    source.skin.clear();
    flt(target.model, 24, 20);
    flt(target.model, 232, 5);
    flt(target.model, 248, 6);
    put(target.model, 244, 0xaabbccdd);
    put(target.model, 260, 0x11223344);
    flt(target.model, 280,
        9); // Same XYZ for vertex1: source normal must survive.
    pose = actor_bind_pose(source);
    pose[0].position = Vec3{30, 6, 0};
    for (float amount : {-500.f, 0.f, 500.f, 1000.f, 1500.f}) {
      auto morphed = morph_actor_model(source, target, pose, amount);
      size_t v = 16 + word(morphed.model, 144);
      ck(read(morphed.model, 24) == 30 + amount * (.001f * (20 - 30)) &&
             read(morphed.model, 28) == 6 + amount * (.001f * (0 - 6)),
         "Morph object translation/order/clamping changed");
      ck(read(morphed.model, v + 64) == 1 + (amount * .001f) * 4 &&
             read(morphed.model, v + 80) == 2 + (amount * .001f) * 4,
         "Morph position/normal interpolation failed");
      ck(read(morphed.model, v + 112) == 3,
         "Same vertex XYZ must retain original normal");
      ck(word(morphed.model, v + 76) == 0xaabbccdd &&
             word(morphed.model, v + 92) == 0x11223344 &&
             morphed.model.at(v + 160) == 255,
         "Morph W lanes or stream terminator changed");
      ck(source.model == raw.model && target.model.size() == 400,
         "Morph changed immutable input");
    }
    auto mismatch = target;
    half(mismatch.model, 174, 2);
    auto morphed = morph_actor_model(source, mismatch, pose, 500);
    ck(read(morphed.model, 24) == 25 && word(morphed.model, 144) == 152 &&
           read(morphed.model, 232) == 1,
       "Count mismatch must retain geometry but update object position");
    auto hidden = source;
    put(hidden.model, 16, 14);
    auto invalid = target;
    invalid.model.clear();
    morphed = morph_actor_model(hidden, invalid, pose,
                                std::numeric_limits<float>::quiet_NaN());
    ck(morphed.model == hidden.model,
       "Skipped morph must ignore unused target/amount/pose");
    morphed = morph_actor_model(source, invalid, pose, 500, 1, 1);
    ck(morphed.model == source.model,
       "Selected absent-model suffix consumed unused target");
    bad([&] { morph_actor_model(source, target, pose, 500, 2, 1); });
    bad([&] { morph_actor_model(raw, raw, pose, 500); });
    bad([&] {
      morph_actor_model(source, target, pose,
                        std::numeric_limits<float>::infinity());
    });
    auto truncated = target;
    truncated.model.resize(250);
    bad([&] { morph_actor_model(source, truncated, pose, 500); });
    ck(raw.model == fixture().model, "Preparation changed original input");
    ScenarioState state;
    state.enemies.resize(128);
    auto &root = state.enemies[0];
    root.flags = 0x1001;
    root.id = 1;
    root.zombie_work.emplace();
    root.model = std::make_shared<ActorModel>(source);
    root.joint_pose = actor_bind_pose(source);
    root.model_work =
        calculate_actor_model_work(source, root.joint_pose, {}, placement);
    const auto retained = root.model_work->joints[0].matrix;
    root.morph_models = std::make_shared<ActorMorphModels>(ActorMorphModels{
        prepare_actor_skin(source), prepare_actor_skin(target)});
    root.model_flags = 2;
    root.shape_count = 1000;
    auto drawn = draw_zombie_model(state, 0);
    ck(drawn.meshes[0].triangles[0].corners[0].position.x == 115,
       "Cached zombie mouth target drawing failed");
    ck(root.model_work->joints[0].matrix == retained &&
           root.joint_pose[0].position->x == 10 &&
           root.model->model == source.model,
       "Zombie drawing mutated retained source/cache/pose");
    root.shape_count = 0;
    drawn = draw_zombie_model(state, 0);
    ck(drawn.meshes[0].triangles[0].corners[0].position.x == 111,
       "Zero shape must retain cached base geometry");
    root.morph_models.reset();
    root.shape_count = std::numeric_limits<float>::quiet_NaN();
    state.flags[5][0] |= 0x20;
    drawn = draw_zombie_model(state, 0);
    ck(drawn.meshes[0].triangles[0].corners[0].position.x == 111,
       "Source st_flg20 must bypass unused mouth shape/models");
    state.flags[5][0] &= ~0x20u;
    bad([&] { draw_zombie_model(state, 0); });
    root.model_flags = 0;
    auto &linked = state.enemies[1];
    linked.flags = 0x81;
    linked.id = 1;
    linked.zombie_work.emplace();
    linked.pose_owner = 0;
    drawn = draw_zombie_model(state, 1);
    ck(drawn.meshes[0].triangles[0].corners[0].position.x == 111,
       "Linked work must draw owner cache");
    linked.model_flags = 4;
    bad([&] { draw_zombie_model(state, 1); });
    linked.flags = 0;
    root.morph_models = std::make_shared<ActorMorphModels>(ActorMorphModels{
        prepare_actor_skin(source), prepare_actor_skin(target)});
    root.model_flags = 2;
    root.shape_count = 1000;
    RoomRuntime runtime;
    runtime.scenario = state;
    auto composed = compose_room_runtime(runtime, 2, 0);
    const auto root_mesh_count = composed.meshes.size();
    ck(composed.meshes[0].triangles[0].corners[0].position.x == 115,
       "Runtime must compose initialized zombie morph from its cached "
       "matrices");
    runtime.scenario.enemies[1].flags = 0x81;
    runtime.scenario.enemies[1].model_flags = 0;
    runtime.scenario.enemies[1].linked_parent = 0;
    runtime.scenario.enemies[0].state_flags |= 0x1000000;
    composed = compose_room_runtime(runtime, 2, 0);
    ck(composed.meshes.empty(),
       "Source stflg1000000 must suppress the parent and its linked body");
    runtime.scenario.enemies[0].state_flags &= ~0x1000000u;
    composed = compose_room_runtime(runtime, 2, 0);
    ck(composed.meshes.size() == root_mesh_count * 2,
       "Source linked-body drawing must resume when parent gate clears");
    std::cout << "Source skin preparation and model morphology passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
