#include "face_deformation.hpp"
#include "face_fixtures.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
bool constrain_allocations = false;
struct AllocationGuard {
  AllocationGuard() { constrain_allocations = true; }
  ~AllocationGuard() { constrain_allocations = false; }
};
} // namespace
void *operator new(size_t n) {
  if (constrain_allocations && n > 4096)
    throw std::bad_alloc();
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
using namespace ashford;
using namespace face_test;
namespace {
Bytes deformation_fixture() {
  auto m = mask_fixture();
  m.resize(64 + 4 + 8 + 40 + 16 + 8);
  put(m, 0x10, 1);
  put(m, 0x14, 2);
  put(m, 0x18, 2);
  put(m, 0x1c, 1);
  half(m, 64, 2);
  m[66] = 2;
  m[67] = 2;
  put(m, 68, 0);
  put(m, 72, 1);
  // Two active connections: coefficients .25 and .75, opposite signed inputs.
  put(m, 76, 0);
  flt(m, 80, 4);
  flt(m, 92, 1);
  put(m, 96, 1);
  flt(m, 104, 4);
  flt(m, 112, 3);
  put(m, 116, 1);
  flt(m, 120, 1);
  put(m, 124, 2);
  flt(m, 128, 0.5f);
  put(m, 132, 2);
  flt(m, 136, 0.5f);
  return m;
}
Vec3 vertex(const PreparedFaceModel &face, size_t start, size_t i) {
  const auto &b = face.actor.model;
  auto p = start + 64 + i * 32;
  return {value(b, p), value(b, p + 4), value(b, p + 8)};
}
void point_near(Vec3 a, Vec3 b) {
  near(a.x, b.x);
  near(a.y, b.y);
  near(a.z, b.z);
}
} // namespace
int main() {
  try {
    auto actor = fixture();
    auto tongue_descriptor = 16 + word(actor.model, 16 + 8 * 52 + 4);
    auto tongue_vertices = 16 + word(actor.model, tongue_descriptor);
    flt(actor.model, tongue_vertices + 128 + 8, 2);
    const auto source_actor_model = actor.model;
    auto mask = deformation_fixture();
    auto face = prepare_face_model(actor, mask);
    const auto initial = face.actor.model;
    auto data = decode_face_deformation(mask, face);
    ck(data.muscles.size() == 1 && data.jaw.size() == 2 &&
           data.tongue.size() == 1,
       "Facial table decoding mismatch");
    FaceParameters p;
    p.muscle[0] = 1;
    p.muscle[1] = -1;
    p.tongue.z = 2;
    p.eye = {0, 0, -100};
    deform_face(face, data, p);
    // Source face vertex2=(0,1,0), muscle ->(1,-2,0), jaw rate .5 no rotation.
    point_near(vertex(face, face.face_geometry.destination_vertices, 2),
               {1, -2, 0});
    point_near(vertex(face, face.tongue_geometry.destination_vertices, 2),
               {0, 1, 4});
    auto first = face.actor.model;
    deform_face(face, data, p);
    ck(first == face.actor.model,
       "Facial animation accumulated between equal frames");
    p.eye = {-100, -100, -100};
    deform_face(face, data, p);
    auto table = 16 + word(face.actor.model, 12);
    ck(int32_t(word(face.actor.model, table + face.eyes[0]->parent * 52 + 20)) <
           0,
       "Eye pitch lost original signed 32-bit angle storage");
    p.eye = {0, 0, -100};
    p.muscle.fill(0);
    p.jaw_translation = 2;
    deform_face(face, data, p);
    // Fractional jaw rate scales its angle, never the jaw translation.
    point_near(vertex(face, face.face_geometry.destination_vertices, 1),
               {3, 0, 0});
    point_near(vertex(face, face.face_geometry.destination_vertices, 2),
               {2, 1, 0});
    p.jaw_translation = 0;
    p.jaw_angle = 90;
    p.tongue = {90, 0, 1};
    deform_face(face, data, p);
    // Jaw -90 at half rate; tongue +90 at half rate.
    auto jaw = vertex(face, face.face_geometry.destination_vertices, 2);
    near(jaw.x, 0);
    near(jaw.y, 0.7071f);
    near(jaw.z, -0.7071f);
    auto tongue = vertex(face, face.tongue_geometry.destination_vertices, 2);
    near(tongue.x, 0);
    near(tongue.y, -0.7071f);
    near(tongue.z, 2.1212f);
    auto body =
        pose_actor(face.actor, actor_bind_pose(face.actor), {100, 200, 300});
    point_near(body.meshes[0].triangles[0].corners[0].position,
               {111, 200, 300});
    // Source geometry must survive all updates and render preparation.
    for (size_t i = 0; i < face.face_geometry.vertex_count * 32; ++i)
      ck(face.actor.model[face.face_geometry.source_vertices + 64 + i] ==
             initial[face.face_geometry.source_vertices + 64 + i],
         "Facial source mutated");
    const auto before_error = face.actor.model;
    bad([&] {
      auto invalid = data;
      invalid.jaw[1].vertex = 999;
      deform_face(face, invalid, p);
    });
    ck(face.actor.model == before_error, "Failed facial update modified actor");
    bad([&] {
      auto invalid = p;
      invalid.eye.x = INFINITY;
      deform_face(face, data, invalid);
    });
    ck(face.actor.model == before_error, "Nonfinite update modified actor");
    auto missing_eye = face;
    missing_eye.eye_matrices[1].reset();
    const auto missing_eye_before = missing_eye.actor.model;
    bad([&] { deform_face(missing_eye, data, p); });
    ck(missing_eye.actor.model == missing_eye_before,
       "Late eye failure committed earlier muscle/jaw writes");
    auto reflected_actor = actor;
    flt(reflected_actor.model, 16 + 40, -1);
    auto reflected_face = prepare_face_model(reflected_actor, mask);
    ck(reflected_face.eye_matrices[0]->at(10) == -1,
       "Eye matrix preparation ignored original scale");
    auto eye_parameters = p;
    eye_parameters.eye = {0, 100, -100};
    deform_face(reflected_face, data, eye_parameters);
    auto reflected_table = 16 + word(reflected_face.actor.model, 12);
    ck(int32_t(word(reflected_face.actor.model,
                    reflected_table + reflected_face.eyes[0]->parent * 52 +
                        20)) < 0,
       "Eye pitch ignored captured reference direction");
    bad([&] {
      auto m = mask;
      m[66] = 33;
      decode_face_deformation(m, face);
    });
    bad([&] {
      auto m = mask;
      put(m, 76, 40);
      decode_face_deformation(m, face);
    });
    bad([&] {
      auto m = mask;
      half(m, 64, 3);
      decode_face_deformation(m, face);
    });
    bad([&] {
      auto m = mask;
      m.resize(m.size() - 1);
      decode_face_deformation(m, face);
    });
    auto truncated = mask_fixture();
    truncated.resize(64 + 1000 * 4);
    put(truncated, 0x10, 1000);
    for (size_t i = 0; i < 1000; ++i) {
      truncated[64 + i * 4 + 2] = 32;
      truncated[64 + i * 4 + 3] = 127;
    }
    bad([&] {
      AllocationGuard guard;
      decode_face_deformation(truncated, face);
    });
    ck(mask == deformation_fixture() && actor.model == source_actor_model,
       "Retail input changed");
    std::cout << "Facial muscle jaw tongue deformation and rollback passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
