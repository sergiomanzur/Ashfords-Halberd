#include "face_fixtures.hpp"
#include <iostream>
using namespace ashford;
using namespace face_test;
int main() {
  try {
    auto a = fixture();
    auto mask = mask_fixture();
    const auto original = a;
    auto f = prepare_face_model(a, mask);
    ck(actor_bind_pose(f.actor).size() == 16, "Facial clones were not added");
    ck(f.original_to_runtime ==
           std::vector<size_t>{0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 15},
       "Facial flattening order mismatch");
    ck(f.tooth.parent == 7 && f.tooth.clone == 8 && f.tongue.parent == 9 &&
           f.tongue.clone == 10,
       "Tooth/tongue bindings mismatch");
    ck(f.eyes[0]->parent == 11 && f.eyes[0]->clone == 12 &&
           f.eyes[1]->parent == 13 && f.eyes[1]->clone == 14,
       "Eye bindings mismatch");
    ck(f.face_joint == 6 && a.model == original.model &&
           a.skin == original.skin && mask == mask_fixture(),
       "Source assets changed");
    auto &b = f.actor.model;
    auto table = 16 + word(b, 12);
    ck(word(b, table + 7 * 52) == 0xa000000c && word(b, table + 8 * 52) == 6,
       "Facial parent/clone flags mismatch");
    ck(f.face_geometry.source_vertices !=
               f.face_geometry.destination_vertices &&
           f.tongue_geometry.source_vertices !=
               f.tongue_geometry.destination_vertices,
       "Facial buffers alias");
    // Body root points at a descriptor, so inspect via its vertex pointer.
    auto body_vertices = 16 + word(b, 16 + word(b, table + 4));
    near(value(b, body_vertices + 64), -1);
    near(value(b, body_vertices + 68), -5);
    auto scene = pose_actor(f.actor, actor_bind_pose(f.actor), {100, 200, 300});
    ck(scene.meshes.size() == 6, "Facial wrapped meshes drawn more than once");
    auto p = scene.meshes[0].triangles[0].corners[0].position;
    near(p.x, 111);
    near(p.y, 200);
    near(p.z, 300);
    for (size_t i = 0; i < 16; ++i)
      ck(f.work.joints[i].flags == (i >= 6 && i <= 14 ? 4u : 0u),
         "Initial facial motion mask mismatch");
    // Hand-derived half blend between translated and quarter-turned joint.
    auto weighted = a;
    for (size_t i = 0; i < 3; ++i)
      put(weighted.skin, 16 + i * 4, 0x0b041234);
    auto blended = prepare_face_model(weighted, mask);
    auto joints = actor_bind_pose(blended.actor);
    joints[15].angles = std::array<uint16_t, 3>{0, 16384, 0};
    auto blend_scene = pose_actor(blended.actor, joints, {100, 200, 300});
    auto blend_point = blend_scene.meshes[0].triangles[0].corners[0].position;
    near(blend_point.x, 111.5f);
    near(blend_point.y, 200);
    near(blend_point.z, 300.5f);
    auto rotated = pose_actor(blended.actor, joints, {100, 200, 300}, 16384);
    auto rotated_point = rotated.meshes[0].triangles[0].corners[0].position;
    near(rotated_point.x, 100.5f);
    near(rotated_point.y, 200);
    near(rotated_point.z, 288.5f);
    const auto fm_skin =
        decode_skin(blended.actor.skin, 16, SkinLayout::FaceModel);
    ck(fm_skin[0].vertices[0].joint == 15 &&
           fm_skin[0].vertices[0].auxiliary == 0x1234 &&
           fm_skin[11].flags == 0 && fm_skin[14].flags == 0,
       "FM remapping lost packed influence metadata");
    near(fm_skin[0].vertices[0].rotation_weight, 0.5f);
    auto angled = a;
    auto face_descriptor = 16 + word(angled.model, 16 + 6 * 52 + 4);
    auto face_vertices = 16 + word(angled.model, face_descriptor);
    flt(angled.model, face_vertices + 96, -1);
    flt(angled.model, face_vertices + 100, 1);
    flt(angled.model, face_vertices + 104, std::sqrt(2.0f));
    put(angled.model, 16 + 7 * 52 + 28, 4096);
    auto jaw = prepare_face_model(angled, mask);
    ck(std::abs(jaw.jaw_angles[0] - 8192) <= 1 &&
           std::abs(jaw.jaw_angles[1] + 8192) <= 1,
       "Jaw ratios differ from original atan (not atan2) alignment");
    auto jaw_pose = actor_bind_pose(jaw.actor);
    ck((*jaw_pose[jaw.tooth.parent].angles)[1] == uint16_t(jaw.jaw_angles[0]) &&
           (*jaw_pose[jaw.tooth.parent].angles)[2] ==
               uint16_t(jaw.jaw_angles[1]) &&
           (*jaw_pose[jaw.tooth.clone].angles)[2] == 4096,
       "Jaw alignment overwrote clone rotation");
    auto chained = a;
    put(chained.model, 16 + 6 * 52 + 48, 0xffffffff);
    put(chained.model, 16 + 7 * 52 + 44, 11 * 52);
    flt(chained.model, 16 + 7 * 52 + 12, 2);
    flt(chained.model, 16 + 7 * 52 + 32, 3);
    // No packed influences: this fixture isolates wrapper child preservation.
    chained.skin = Bytes(4 + 12 * 4 + 4);
    std::copy_n(a.skin.begin(), 4, chained.skin.begin());
    put(chained.skin, chained.skin.size() - 4, 0xffffffff);
    auto hierarchy = prepare_face_model(chained, mask);
    auto hierarchy_table = 16 + word(hierarchy.actor.model, 12);
    auto clone_offset = hierarchy_table + hierarchy.tooth.clone * 52;
    auto parent_offset = hierarchy_table + hierarchy.tooth.parent * 52;
    ck(word(hierarchy.actor.model, clone_offset + 44) == 0xffffffff &&
           word(hierarchy.actor.model, clone_offset + 48) ==
               hierarchy_table - 16 + hierarchy.original_to_runtime[11] * 52,
       "Facial wrapper lost the original child chain");
    near(value(hierarchy.actor.model, clone_offset + 12), 0);
    near(value(hierarchy.actor.model, parent_offset + 12), 2);
    near(value(hierarchy.actor.model, clone_offset + 32), 3);
    near(value(hierarchy.actor.model, parent_offset + 32), 1);
    b.at(f.face_geometry.destination_vertices + 64) = 123;
    ck(b.at(f.face_geometry.source_vertices + 64) ==
           original.model.at(f.face_geometry.source_vertices + 64),
       "Mutable facial output overwrites source");
    bad([&] {
      auto m = mask;
      m.resize(63);
      prepare_face_model(a, m);
    });
    bad([&] { prepare_face_model(f.actor, mask); });
    bad([&] {
      auto unconverted = f.actor;
      unconverted.skin_preconverted = false;
      pose_actor(unconverted, actor_bind_pose(unconverted));
    });
    Bytes wide_skin{'S', 'K', 'I', 'N'};
    auto append = [&](uint32_t v) {
      auto offset = wide_skin.size();
      wide_skin.resize(offset + 4);
      put(wide_skin, offset, v);
    };
    append(3);
    append(0);
    append(1);
    append(0xff09bbee);
    for (size_t i = 1; i < 255; ++i)
      append(0);
    append(1);
    append(0);
    append(0xffffffff);
    auto wide = decode_skin(wide_skin, 260, SkinLayout::FaceModel);
    ck(wide[0].vertices[0].joint == 259 && wide[259].flags == 1 &&
           wide[0].vertices[0].auxiliary == 0xbbee,
       "FM packed influence overflowed eight bits");
    bad([&] { decode_skin(wide_skin, 259, SkinLayout::FaceModel); });
    bad([&] {
      auto m = mask;
      m[0] = 'X';
      prepare_face_model(a, m);
    });
    bad([&] {
      auto m = mask;
      m[0x34] = 120;
      prepare_face_model(a, m);
    });
    bad([&] {
      auto m = mask;
      m[0x35] = m[0x36];
      prepare_face_model(a, m);
    });
    bad([&] {
      auto m = mask;
      m[0x38] = 255;
      prepare_face_model(a, m);
    });
    bad([&] {
      auto m = mask;
      half(m, 10, 3);
      prepare_face_model(a, m);
    });
    bad([&] {
      auto m = mask;
      put(m, 0x10, 0xffffffff);
      prepare_face_model(a, m);
    });
    bad([&] {
      auto x = a;
      put(x.model, 16 + 44, 0);
      prepare_face_model(x, mask);
    });
    bad([&] {
      auto x = a;
      auto d = 16 + word(x.model, 16 + 6 * 52 + 4);
      put(x.model, d, word(x.model, 16 + word(x.model, 20)));
      prepare_face_model(x, mask);
    });
    bad([&] {
      auto x = a;
      put(x.skin, 16, 0xff091234);
      prepare_face_model(x, mask);
    });
    std::cout << "Facial preparation and world placement passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
