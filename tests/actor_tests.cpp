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
    Bytes model_bank;
    auto record = [&](const Bytes &bytes) {
      size_t p = model_bank.size();
      model_bank.resize(p + 4);
      put(model_bank, p, uint32_t(bytes.size()));
      model_bank.insert(model_bank.end(), bytes.begin(), bytes.end());
    };
    record({});
    record(a.skin);
    record(Bytes{'M', 'A', 'S', 'K', 7, 8, 9, 10});
    record(a.model);
    size_t sentinel = model_bank.size();
    model_bank.resize(sentinel + 4);
    put(model_bank, sentinel, 0xffffffff);
    auto bank = decode_actor_model_bank(model_bank, 0, model_bank.size());
    ck(bank.models.size() == 1 && bank.models[0].skin == a.skin &&
       bank.models[0].model == a.model && bank.mask[4] == 7);
    bad([&] { decode_actor_model_bank(model_bank, 0, model_bank.size() - 1); });
    bad([&] { decode_actor_model_bank(model_bank, 4, 3); });
    auto invalid_bank = model_bank;
    put(invalid_bank, 4, 0xfffffffe);
    bad([&] { decode_actor_model_bank(invalid_bank, 0, invalid_bank.size()); });
    Bytes dangling(4 + a.skin.size() + 4);
    put(dangling, 0, uint32_t(a.skin.size()));
    std::copy(a.skin.begin(), a.skin.end(), dangling.begin() + 4);
    put(dangling, dangling.size() - 4, 0xffffffff);
    bad([&] { decode_actor_model_bank(dangling, 0, dangling.size()); });
    Bytes room(3000);
    flt(room, 0, 10);
    put(room, 16, 128);
    put(room, 128 + 2 * 4, 1132);
    put(room, 128 + 0x80 + 2 * 4, 3);
    for (size_t index : {size_t(0), size_t(2)}) {
      put(room, 1132 + index * 36, 1);
      put(room, 1136 + index * 36, 67);
    }
    put(room, 20, 1240);
    put(room, 1240, 1300);
    put(room, 1244, 1700);
    put(room, 1248, 2200);
    std::copy(a.model.begin(), a.model.end(), room.begin() + 1300);
    auto textured_bank = model_bank;
    // Null + SKIN + MASK precede the MDL record; give it its own texture list.
    size_t mdl = 4 + 4 + a.skin.size() + 4 + 8 + 4;
    put(textured_bank, mdl + 8, 0);
    std::copy(textured_bank.begin(), textured_bank.end(), room.begin() + 1700);
    std::copy(textured_bank.begin(), textured_bank.end(), room.begin() + 2200);
    put(room, 32, 2800);
    put(room, 2800, 2);
    put(room, 2804, 2850);
    put(room, 2808, 2900);
    put(room, 2850, 0xffffffff);
    put(room, 2900, 0xffffffff);
    auto enemies = decode_room_enemy_models(room);
    ck(enemies.size() == 3 && enemies[1].models.empty());
    ck(enemies[0].models[0].textures == Bytes({255, 255, 255, 255}) &&
       enemies[2].models[0].textures == enemies[0].models[0].textures);
    put(room, 2800, 1);
    bad([&] { decode_room_enemy_models(room); });
    put(room, 2800, 2);
    put(room, 1244, 1248);
    bad([&] { decode_room_enemy_models(room); });
    put(room, 1244, 1700);
    put(room, 2808, 2804);
    bad([&] { decode_room_enemy_models(room); });
    auto multi_room = room;
    multi_room.resize(7000);
    put(multi_room, 1248, 3000);
    Bytes multi_bank;
    for (bool own : {true, false, true}) {
      auto model = a.model;
      put(model, 8, own ? 0 : 0xffffffff);
      size_t p = multi_bank.size();
      multi_bank.resize(p + 4);
      put(multi_bank, p, uint32_t(model.size()));
      multi_bank.insert(multi_bank.end(), model.begin(), model.end());
    }
    multi_bank.resize(multi_bank.size() + 4);
    put(multi_bank, multi_bank.size() - 4, 0xffffffff);
    std::copy(multi_bank.begin(), multi_bank.end(), multi_room.begin() + 1700);
    std::copy(multi_bank.begin(), multi_bank.end(), multi_room.begin() + 3000);
    put(multi_room, 32, 5000);
    put(multi_room, 5000, 5);
    for (size_t i = 0; i < 5; ++i) {
      size_t p = 5500 + i * 40;
      put(multi_room, 5004 + i * 4, uint32_t(p));
      put(multi_room, p, 0x00494c50);
      put(multi_room, p + 4, 0);
      put(multi_room, p + 8, uint32_t(i * 10));
      put(multi_room, p + 32, 0xffffffff);
    }
    put(multi_room, 1136 + 2 * 36, 68);
    auto distinct = decode_room_enemy_models(multi_room);
    ck(distinct[0].models.size() == 3 && distinct[2].models.size() == 3);
    ck(distinct[0].models[0].textures[8] == 10 &&
       distinct[0].models[1].textures == distinct[0].models[0].textures &&
       distinct[0].models[2].textures[8] == 20);
    ck(distinct[2].models[0].textures[8] == 30 &&
       distinct[2].models[1].textures == distinct[2].models[0].textures &&
       distinct[2].models[2].textures[8] == 40);
    put(multi_room, 1136 + 2 * 36, 67);
    multi_room[1132 + 2 * 36 + 9] = 1;
    ck(decode_room_enemy_models(multi_room)[2].models[0].textures[8] == 30);
    multi_room[1132 + 2 * 36 + 9] = 0;
    auto shared_banks = decode_room_enemy_models(multi_room);
    ck(shared_banks[2].models[0].textures[8] == 10 &&
       shared_banks[2].models[2].textures[8] == 20);
    // Retail uses model versions beyond 15. PS2 cache addressing is linear:
    // [1][32][0] aliases [3][0][0] inside the complete allocation.
    auto aliased_room = multi_room;
    put(aliased_room, 1136, 1);
    aliased_room[1132 + 9] = 32;
    put(aliased_room, 1136 + 2 * 36, 3);
    put(aliased_room, 5000, 3); // Room + two packs; alias must consume none.
    auto aliased_banks = decode_room_enemy_models(aliased_room);
    ck(aliased_banks[2].models[0].textures ==
           aliased_banks[0].models[0].textures &&
       aliased_banks[2].models[2].textures ==
           aliased_banks[0].models[2].textures);
    put(aliased_room, 1136, 99);
    aliased_room[1132 + 9] = 16;
    bad([&] { decode_room_enemy_models(aliased_room); });
    auto populated = multi_room;
    populated.resize(9000);
    put(populated, 128 + 3 * 4, 6200);
    put(populated, 128 + 0x80 + 3 * 4, 6);
    put(populated, 128 + 4 * 4, 6400);
    put(populated, 128 + 0x80 + 4 * 4, 2);
    put(populated, 6200 + 4 * 36, 1);
    put(populated, 6204 + 4 * 36, 10);
    put(populated, 6200 + 5 * 36, 1);
    put(populated, 6204 + 5 * 36, 10);
    for (size_t i = 0; i < 2; ++i) {
      put(populated, 6400 + i * 36, 1);
      put(populated, 6404 + i * 36, 10);
    }
    put(populated, 1252, 6500);
    put(populated, 1256, 7000);
    put(populated, 1260, 7200);
    put(populated, 1264, 8000);
    put(populated, 6500, uint32_t(a.skin.size() - 4));
    std::copy(a.skin.begin(), a.skin.end(), populated.begin() + 6504);
    auto item_model = a.model;
    put(item_model, 8, 0);
    std::copy(item_model.begin(), item_model.end(),
              populated.begin() + 6504 + a.skin.size());
    std::copy(item_model.begin(), item_model.end(), populated.begin() + 7000);
    std::copy(item_model.begin(), item_model.end(), populated.begin() + 7200);
    std::copy(item_model.begin(), item_model.end(), populated.begin() + 8000);
    auto complete = decode_room_actor_models(populated);
    ck(complete.objects.size() == 6 && complete.objects[0].model.empty() &&
       complete.objects[4].skin == a.skin &&
       complete.objects[4].textures[8] == 30 &&
       complete.objects[5].textures == complete.objects[4].textures &&
       complete.items.size() == 2 && complete.items[0].textures[8] == 40 &&
       complete.items[0].textures == complete.items[1].textures);
    auto invalid_object = populated;
    put(invalid_object, 6500, 0xffffffff);
    bad([&] { decode_room_actor_models(invalid_object); });
    invalid_object = populated;
    put(invalid_object, 6204 + 4 * 36, 1300);
    bad([&] { decode_room_actor_models(invalid_object); });
    invalid_object = populated;
    put(invalid_object, 6404, 200);
    bad([&] { decode_room_actor_models(invalid_object); });
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
    auto rotated =
        pose_actor_rotated(a, pose, {100, 200, 300}, {16384, 16384, 16384});
    auto rotated_point = rotated.meshes[0].triangles[0].corners[0].position;
    // (12,3,0) -> Rx(12,0,3) -> Ry(3,0,-12) -> Rz(0,3,-12).
    ck(std::abs(rotated_point.x - 100) < 0.0001 &&
       std::abs(rotated_point.y - 203) < 0.0001 &&
       std::abs(rotated_point.z - 288) < 0.0001);
    // Joint output must use the same hierarchy and placement as rendering.
    const auto original_model = a.model, original_skin = a.skin;
    auto joints = actor_joint_matrices(a, pose, {100, 0, 0}, {0, 16384, 0});
    ck(joints.size() == 2);
    ck(std::abs(joints[0][3] - 100) < 0.0001 &&
       std::abs(joints[0][7]) < 0.0001 &&
       std::abs(joints[0][11] + 10) < 0.0001);
    ck(std::abs(joints[1][3] - 100) < 0.0001 &&
       std::abs(joints[1][7] - 2) < 0.0001 &&
       std::abs(joints[1][11] + 10) < 0.0001 &&
       std::abs(joints[1][4] - 1) < 0.0001);
    // Parent rotation transforms the local offset too; XYZ order matters.
    auto linked =
        pose_actor_linked(a, pose, joints[0], {2, 4, 6}, {16384, 16384, 16384});
    auto linked_point = linked.meshes[0].triangles[0].corners[0].position;
    ck(std::abs(linked_point.x - 94) < 0.0001 &&
       std::abs(linked_point.y - 7) < 0.0001 &&
       std::abs(linked_point.z + 12) < 0.0001);
    ck(a.model == original_model && a.skin == original_skin);
    auto rigid = a;
    rigid.skin.clear();
    auto rigid_joints =
        actor_joint_matrices(rigid, pose, {100, 0, 0}, {0, 16384, 0});
    auto rigid_point = pose_actor(rigid, pose, {100, 0, 0}, 16384)
                           .meshes[0]
                           .triangles[0]
                           .corners[0]
                           .position;
    // This fixture's first rigid vertex is (1,0,0) owned by joint zero.
    ck(std::abs(rigid_point.x - (rigid_joints[0][0] + rigid_joints[0][3])) <
           0.0001 &&
       std::abs(rigid_point.z - (rigid_joints[0][8] + rigid_joints[0][11])) <
           0.0001);
    auto siblings = multiple_groups();
    auto sibling_joints = actor_joint_matrices(siblings, {});
    ck(sibling_joints.size() == 3 && sibling_joints[1][3] == 10 &&
       sibling_joints[1][7] == 2 && sibling_joints[2][3] == 13 &&
       sibling_joints[2][7] == 0);
    // Matrix-only queries must not need skin or texture decoding.
    rigid.skin = {1};
    rigid.textures = {1};
    ck(actor_joint_matrices(rigid, pose) == actor_joint_matrices(a, pose));
    bad([&] { actor_joint_matrices(a, std::vector<JointPose>(3)); });
    auto invalid_joint_model = a;
    put(invalid_joint_model.model, 60, 0);
    bad([&] { actor_joint_matrices(invalid_joint_model, pose); });
    put(invalid_joint_model.model, 60, 0xffffffff);
    bad([&] { actor_joint_matrices(invalid_joint_model, pose); });
    bad([&] { actor_joint_matrices(a, pose, {NAN, 0, 0}); });
    auto invalid_parent = joints[0];
    invalid_parent[0] = NAN;
    bad([&] { pose_actor_linked(a, pose, invalid_parent); });
    invalid_parent = joints[0];
    invalid_parent[12] = 1;
    bad([&] { pose_actor_linked(a, pose, invalid_parent); });
    invalid_parent = joints[0];
    invalid_parent[15] = 0;
    bad([&] { pose_actor_linked(a, pose, invalid_parent); });
    bad([&] { pose_actor_linked(a, pose, joints[0], {INFINITY, 0, 0}); });
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
