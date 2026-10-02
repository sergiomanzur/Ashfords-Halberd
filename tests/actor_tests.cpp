#include "actor.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool b) {
  if (!b)
    throw std::runtime_error("Actor check failed");
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed);
}
template <class F> void overlapping(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &e) {
    rejected = std::string(e.what()).find("Overlapping skin vertex buffers") !=
               std::string::npos;
  }
  ck(rejected);
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
void half(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
void flt(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
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
  put(b, 12, 0);
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
  b[328] = 255;
  b[344] = 66;
  half(b, 346, 11);
  half(b, 348, 1);
  half(b, 350, 3);
  for (size_t i = 0; i < 3; ++i)
    half(b, 352 + i * 6, uint16_t(i));
  b[370] = 255;
  auto &skin = a.skin;
  skin.resize(40);
  skin[0] = 'S';
  skin[1] = 'K';
  skin[2] = 'I';
  skin[3] = 'N';
  put(skin, 4, 3);
  put(skin, 8, 0);
  put(skin, 12, 3);
  for (size_t i = 0; i < 3; ++i)
    put(skin, 16 + i * 4, 0x01090000);
  put(skin, 28, 1);
  put(skin, 32, 0);
  put(skin, 36, 0xffffffff);
  return a;
}
ActorModel multiple_groups() {
  auto a = fixture();
  auto original = a.model;
  a.model.resize(772);
  auto &b = a.model;
  half(b, 6, 3);
  put(b, 12, 400);
  std::copy(original.begin() + 16, original.begin() + 120, b.begin() + 416);
  std::copy(original.begin() + 16, original.begin() + 68, b.begin() + 520);
  put(b, 460, 452);
  put(b, 464, 0xffffffff);
  put(b, 512, 0xffffffff);
  put(b, 516, 504);
  put(b, 524, 556);
  flt(b, 528, 3);
  flt(b, 532, 0);
  put(b, 564, 0xffffffff);
  put(b, 568, 0xffffffff);
  std::copy(original.begin() + 144, original.begin() + 168, b.begin() + 572);
  put(b, 572, 580);
  std::copy(original.begin() + 168, original.begin() + 344, b.begin() + 596);
  a.skin.resize(64);
  put(a.skin, 36, 3);
  put(a.skin, 40, 1);
  put(a.skin, 44, 3);
  for (size_t i = 0; i < 3; ++i)
    put(a.skin, 48 + i * 4, 0x02090000);
  put(a.skin, 60, 0xffffffff);
  return a;
}
int main() {
  try {
    auto a = fixture();
    std::vector<JointPose> pose(2);
    pose[1].angles = std::array<uint16_t, 3>{0, 0, 16384};
    auto scene = pose_actor(a, pose);
    ck(scene.meshes.size() == 1);
    auto p = scene.meshes[0].triangles[0].corners[0].position;
    ck(std::abs(p.x - 12) < 0.0001 && std::abs(p.y - 3) < 0.0001 &&
       scene.meshes[0].object >= 512);
    put(a.skin, 16, 0x01040000);
    p = pose_actor(a, pose).meshes[0].triangles[0].corners[0].position;
    ck(std::abs(p.x - 11.5) < 0.0001 && std::abs(p.y - 1.5) < 0.0001);
    a = fixture();
    p = pose_actor(a, pose, {100, 0, 0}, 16384)
            .meshes[0]
            .triangles[0]
            .corners[0]
            .position;
    ck(std::abs(p.x - 100) < 0.0001 && std::abs(p.y - 3) < 0.0001 &&
       std::abs(p.z + 12) < 0.0001);
    auto moving = pose;
    moving[1].position = Vec3{0, 4, 0};
    p = pose_actor(fixture(), moving)
            .meshes[0]
            .triangles[0]
            .corners[0]
            .position;
    ck(std::abs(p.x - 12) < 0.0001 && std::abs(p.y - 5) < 0.0001);
    auto combined = pose;
    combined[0].angles = std::array<uint16_t, 3>{0, 0, 16384};
    combined[1].angles = std::array<uint16_t, 3>{16384, 16384, 16384};
    auto blended = fixture();
    put(blended.skin, 16, 0x01040000);
    p = pose_actor(blended, combined)
            .meshes[0]
            .triangles[0]
            .corners[0]
            .position;
    ck(std::abs(p.x - 10) < 0.0001 && std::abs(p.y - 0.5) < 0.0001 &&
       std::abs(p.z + 0.5) < 0.0001);
    auto textured = fixture();
    Bytes t(68);
    put(t, 0, 0x324d4954);
    t[4] = 4;
    half(t, 6, 1);
    put(t, 16, 52);
    put(t, 24, 4);
    half(t, 28, 48);
    t[35] = 3;
    half(t, 36, 1);
    half(t, 38, 1);
    t[64] = 255;
    t[65] = 255;
    t[66] = 255;
    t[67] = 128;
    textured.textures.resize(32);
    put(textured.textures, 0, 0x324d4954);
    put(textured.textures, 4, 68);
    textured.textures.insert(textured.textures.end(), t.begin(), t.end());
    textured.textures.resize(104);
    put(textured.textures, 100, 0xffffffff);
    Bytes uv(textured.model.begin() + 344, textured.model.begin() + 372);
    textured.model[344] = 64;
    half(textured.model, 346, 5);
    half(textured.model, 352, 0);
    half(textured.model, 354, 1);
    half(textured.model, 356, 2);
    std::copy(uv.begin(), uv.end(), textured.model.begin() + 358);
    auto mixed = pose_actor(textured, pose);
    ck(mixed.meshes[0].triangles.size() == 2 &&
       mixed.meshes[0].triangles[0].texture == 0xffff &&
       mixed.meshes[0].triangles[1].texture == 0);
    auto multi_pose = pose;
    multi_pose.push_back(pose[1]);
    auto groups = pose_actor(multiple_groups(), multi_pose);
    ck(groups.meshes.size() == 2);
    auto first_group = groups.meshes[0].triangles[0].corners[0].position;
    auto second_group = groups.meshes[1].triangles[0].corners[0].position;
    ck(std::abs(first_group.x - 12) < 0.0001 &&
       std::abs(first_group.y - 3) < 0.0001);
    ck(std::abs(second_group.x - 13) < 0.0001 &&
       std::abs(second_group.y - 1) < 0.0001);
    auto shared = multiple_groups();
    put(shared.model, 524, 128);
    overlapping([&] { pose_actor(shared, multi_pose); });
    shared.skin.resize(44);
    put(shared.skin, 36, 0);
    put(shared.skin, 40, 0xffffffff);
    overlapping([&] { pose_actor(shared, multi_pose); });
    auto partial = multiple_groups();
    put(partial.model, 572, 184);
    partial.model[200] = 51;
    half(partial.model, 202, 39);
    half(partial.model, 206, 3);
    overlapping([&] { pose_actor(partial, multi_pose); });
    a.skin.clear();
    p = pose_actor(a, pose).meshes[0].triangles[0].corners[0].position;
    ck(p.x == 11 && p.y == 0);
    a = fixture();
    put(a.model, 60, 0);
    bad([&] { pose_actor(a, pose); });
    a = fixture();
    put(a.skin, 12, 2);
    bad([&] { pose_actor(a, pose); });
    bad([&] { pose_actor(fixture(), std::vector<JointPose>(3)); });
    a = fixture();
    put(a.model, 60, 0xffffffff);
    bad([&] { pose_actor(a, pose); });
    a = fixture();
    put(a.model, 144, 0xfffffffe);
    bad([&] { pose_actor(a, pose); });
    a = fixture();
    put(a.skin, 28, 0);
    bad([&] { pose_actor(a, pose); });
    a = fixture();
    a.model.resize(250);
    bad([&] { pose_actor(a, pose); });
    auto nonfinite = pose;
    nonfinite[1].position = Vec3{INFINITY, 0, 0};
    bad([&] { pose_actor(fixture(), nonfinite); });
    bad([&] { pose_actor(fixture(), pose, {NAN, 0, 0}); });
    uint32_t random = 0x7822;
    for (int trial = 0; trial < 600; ++trial) {
      auto mutated = fixture();
      random = random * 1664525u + 1013904223u;
      mutated.model[random % mutated.model.size()] ^= uint8_t(random >> 24);
      try {
        pose_actor(mutated, pose);
      } catch (const std::runtime_error &) {
      }
    }
    std::cout << "Actor checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
