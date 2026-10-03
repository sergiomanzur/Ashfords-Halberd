#pragma once
#include "actor.hpp"

namespace ashford {
struct FaceGeometry {
  size_t source_descriptor = 0, source_vertices = 0;
  size_t destination_descriptor = 0, destination_vertices = 0;
  size_t vertex_count = 0;
};
struct FacePart {
  size_t parent = 0, clone = 0;
  Vec3 original_position{};
  std::array<int32_t, 3> original_angles{};
};
struct PreparedFaceModel {
  ActorModel actor;
  ActorModelWork work;
  std::vector<size_t> original_to_runtime;
  size_t face_joint = 0;
  FaceGeometry face_geometry, tongue_geometry;
  FacePart tooth, tongue;
  std::array<std::optional<FacePart>, 9> eyes;
  // Captured during eye wrapping, before jaw alignment; includes XYZ scale.
  std::array<std::optional<ModelMatrix>, 9> eye_matrices;
  Vec3 jaw_anchor{};
  std::array<int32_t, 2> jaw_angles{}; // Source parent Y and Z angles.
};
// face_bh.c preparation for the selected four-added-object FM skin layout.
// Owns all model storage; source assets remain unchanged. Animation is
// separate.
PreparedFaceModel prepare_face_model(const ActorModel &actor,
                                     const Bytes &mask);
} // namespace ashford
