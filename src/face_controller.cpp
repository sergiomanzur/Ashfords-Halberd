#include "face_controller.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp face.c bhInitMask/bhControlMask.
namespace ashford {
namespace {
uint32_t word(const Bytes &b, size_t p) {
  if (p > b.size() || b.size() - p < 4)
    throw std::runtime_error("Facial control offset outside data");
  return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
         uint32_t(b[p + 3]) << 24;
}
void put(Bytes &b, size_t p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(value >> (i * 8));
}
void layout(const PreparedFaceModel &model) {
  if (model.tooth.parent != 7 || model.tongue.parent != 11 || !model.eyes[0] ||
      model.eyes[0]->parent != 9 || !model.eyes[1] ||
      model.eyes[1]->parent != 13)
    throw std::runtime_error("Unsupported facial controller runtime layout");
}
} // namespace
FaceRuntime
initialize_face_runtime(const ActorModel &actor, const Bytes &mask,
                        std::shared_ptr<const std::vector<Bytes>> clips) {
  FaceRuntime result;
  result.model = prepare_face_model(actor, mask);
  layout(result.model);
  result.deformation = decode_face_deformation(mask, result.model);
  const auto records = face_track_records(mask);
  auto keys = decode_face_parameter_records(records);
  if (keys.empty())
    throw std::runtime_error("Facial controller requires shape keys");
  result.controller.original =
      std::make_shared<const std::vector<FaceKeyframe>>(std::move(keys));
  result.controller.lip_shapes =
      std::make_shared<const std::vector<LipShape>>(decode_lip_shapes(records));
  result.controller.clips = std::move(clips);
  select_face_lip(result.controller, 0);
  // bhInitMask intentionally starts last_frame=0, phase=1, no animation flags.
  return result;
}
bool select_face_mask(FaceController &control, const std::vector<Bytes> &masks,
                      size_t index, int32_t start) {
  if (start < 0)
    throw std::runtime_error("Negative facial starting frame");
  if (masks.empty()) {
    control.start_frame = start;
    return false;
  }
  // Original traversal updates the pointer for each visited record, even
  // when the requested index later reaches the bank sentinel.
  const auto &bytes = masks[std::min(index, masks.size() - 1)];
  const auto count = word(bytes, 4);
  if (!count || count > 1000000 || size_t(count) * 164 > bytes.size() - 8)
    throw std::runtime_error("Invalid external facial track extent");
  auto selected = std::make_shared<FaceShapeTrack>();
  selected->actor_id = bytes[0];
  selected->keys = decode_face_parameter_records(
      Bytes(bytes.begin() + 8, bytes.begin() + 8 + size_t(count) * 164));
  control.pending = std::move(selected);
  control.start_frame = start;
  if (index >= masks.size())
    return false;
  control.mask_number = index;
  control.flags |= 8;
  return true;
}
bool select_face_lip(FaceController &control, size_t index) {
  if (!control.clips || control.clips->empty())
    return false;
  control.selected_clip = std::min(index, control.clips->size() - 1);
  if (index >= control.clips->size())
    return false;
  control.lip_number = index;
  // Selection does not interrupt already active LIP_WORK playback.
  return true;
}
bool tick_face_runtime(FaceRuntime &face, std::vector<JointPose> &pose,
                       uint16_t actor_id, uint32_t &state_flags,
                       uint32_t game_flags, int loop_now, int loop_count) {
  if (loop_count < 1 || loop_now < 0 || loop_now >= loop_count)
    throw std::runtime_error("Invalid facial update loop");
  FaceController next = face.controller;
  if (next.phase == 0) {
    face.controller.phase = 1;
    return false;
  }
  if (next.phase != 1 || !next.original || next.original->empty() ||
      next.frame < 0 || next.frame > 32767 || next.last_frame < 0 ||
      next.last_frame > 32767 || next.idle_count < 0)
    throw std::runtime_error("Invalid facial controller state");
  uint32_t flags = state_flags;
  if (!(flags & 0x80)) {
    if (next.idle_count == std::numeric_limits<int32_t>::max())
      throw std::runtime_error("Facial idle counter overflow");
    if (++next.idle_count > 1)
      flags |= 0x80;
  }
  if (game_flags & 0x200)
    flags &= ~0x80u;
  if (next.flags & 8) {
    next.flags &= ~8u;
    next.idle_count = 0;
    flags &= ~0x80u;
    if (!next.pending)
      throw std::runtime_error("Missing external facial request");
    if (!next.pending->actor_id || next.pending->actor_id == actor_id) {
      if (next.pending->keys.empty())
        throw std::runtime_error("Empty external facial track");
      next.selected = next.pending;
      next.frame = next.start_frame;
      next.last_frame = next.selected->keys.back().frame;
      if (next.frame > next.last_frame)
        next.frame = 0;
    }
  }
  if (next.flags & 4) {
    if (!next.lip.active) {
      if (!next.clips || !next.selected_clip ||
          *next.selected_clip >= next.clips->size() || !next.lip_shapes ||
          next.lip_group > next.lip_shapes->size() / 6 ||
          next.lip_group * 6 >= next.lip_shapes->size())
        throw std::runtime_error("Missing facial lipsync binding");
      next.lip =
          start_lip_playback(next.clips->at(*next.selected_clip),
                             next.lip_shapes->size() - next.lip_group * 6);
    }
  } else
    next.lip.active = false;
  const auto &track = next.selected ? next.selected->keys : *next.original;
  next.parameters = sample_face_track(track, float(next.frame));
  if (next.lip.active) {
    flags &= ~0x80u;
    next.idle_count = 0;
    if (!next.lip_shapes || next.lip_group > next.lip_shapes->size() / 6 ||
        next.lip_group * 6 >= next.lip_shapes->size())
      throw std::runtime_error("Facial lip group outside bank");
    const auto start = next.lip_group * 6;
    // The source changes the base by 984 bytes, then indexes remaining records.
    const std::vector<LipShape> shapes(next.lip_shapes->begin() + start,
                                       next.lip_shapes->end());
    tick_lip_playback(next.parameters, shapes, next.lip);
    if (!next.lip.active)
      next.flags &= ~4u;
  }
  if ((next.flags & 1) && !(next.flags & 0x10)) {
    ++next.frame;
    if (next.frame >= next.last_frame) {
      if (!(next.flags & 2)) {
        next.frame = std::max(0, next.last_frame - 1);
        next.flags &= ~1u;
      } else
        next.frame = 0;
    } else {
      flags &= ~0x80u;
      next.idle_count = 0;
    }
  }
  if (next.flags & 0x20) {
    flags &= ~0x80u;
    next.idle_count = 0;
    next.flags &= ~0x31u;
    next.selected.reset();
    next.frame = 0;
    next.last_frame = next.original->back().frame;
  }
  const bool draw = loop_now == loop_count - 1 && !(flags & 0x80);
  if (draw) {
    layout(face.model);
    const auto count = actor_bind_pose(face.model.actor).size();
    if (pose.size() != count || count < 14 || !pose[7].angles ||
        !pose[11].angles || !pose[9].angles || !pose[13].angles)
      throw std::runtime_error("Invalid facial runtime pose");
    auto prepared = face.model;
    deform_face(prepared, face.deformation, next.parameters);
    auto updated = pose;
    const size_t header = uint16_t(prepared.actor.model.at(4)) |
                          uint16_t(prepared.actor.model.at(5)) << 8;
    const size_t table = header + word(prepared.actor.model, 12);
    for (size_t joint : {size_t(7), size_t(11)})
      (*updated[joint].angles)[0] =
          uint16_t(word(prepared.actor.model, table + joint * 52 + 20));
    for (size_t joint : {size_t(9), size_t(13)}) {
      const size_t p = table + joint * 52;
      for (size_t axis = 0; axis < 2; ++axis) {
        const uint32_t inverted =
            0u - word(prepared.actor.model, p + 20 + axis * 4);
        put(prepared.actor.model, p + 20 + axis * 4, inverted);
        (*updated[joint].angles)[axis] = uint16_t(inverted);
      }
      // bhControlMask copies only eye X/Y into the separate runtime table.
      put(prepared.actor.model, p + 28, word(face.model.actor.model, p + 28));
    }
    face.model.actor.model = std::move(prepared.actor.model);
    pose = std::move(updated);
  }
  face.controller = std::move(next);
  state_flags = flags;
  return draw;
}
} // namespace ashford
