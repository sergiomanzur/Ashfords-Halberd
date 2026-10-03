#pragma once
#include "face_model.hpp"
#include "face_parameters.hpp"
namespace ashford {
struct FaceConnection {
  uint32_t parameter = 0;
  Vec3 vector{};
  float scale = 0;
};
struct FaceVertexBinding {
  size_t vertex = 0;
  std::vector<FaceConnection> connections;
  std::vector<size_t> normal_vertices;
};
struct FaceWeightedVertex {
  size_t vertex = 0;
  float rate = 0;
};
struct FaceDeformationData {
  std::vector<FaceVertexBinding> muscles;
  std::vector<FaceWeightedVertex> jaw, tongue;
};
FaceDeformationData decode_face_deformation(const Bytes &mask,
                                            const PreparedFaceModel &face);
// Selected original mode 1: preserve source normals. Recalculate model work
// after this update, as bhControlMask precedes the original enemy callback.
// Errors leave all private model bytes and caller parameters unchanged.
void deform_face(PreparedFaceModel &face, const FaceDeformationData &data,
                 const FaceParameters &parameters);
} // namespace ashford
