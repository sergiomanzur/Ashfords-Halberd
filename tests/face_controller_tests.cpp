#include "face_controller.hpp"
#include "face_fixtures.hpp"
#include "scenario.hpp"
#include <iostream>
using namespace ashford;
using namespace face_test;
namespace {
Bytes mask() {
  auto b = mask_fixture();
  b[0x35] = 4;
  b[0x37] = 3; // Original selected runtime parents: 7,11,9,13.
  put(b, 0x18, 1);
  put(b, 0x20, 2);
  b.resize(64 + 8 + 2 * 164);
  put(b, 64, 1);
  flt(b, 68, 1);
  for (size_t i = 0; i < 2; ++i) {
    const size_t p = 72 + i * 164;
    flt(b, p + 132, float(i * 4));
    flt(b, p + 136, 1);
    flt(b, p + 140, 1);
    flt(b, p + 144, -100);
    flt(b, p + 156, 1);
    half(b, p + 160, uint16_t(i * 4));
  }
  return b;
}
Bytes external(uint8_t id) {
  Bytes b(8 + 2 * 164);
  b[0] = id;
  put(b, 4, 2);
  for (size_t i = 0; i < 2; ++i) {
    size_t p = 8 + i * 164;
    flt(b, p + 132, float(10 + i * 4));
    flt(b, p + 136, 2);
    flt(b, p + 140, 1);
    flt(b, p + 144, -100);
    flt(b, p + 156, 1);
    half(b, p + 160, uint16_t(i * 4));
  }
  return b;
}
Bytes room_fixture(const ActorModel &raw, bool with_mask = true) {
  Bytes b(10000);
  flt(b, 0, 10);
  put(b, 16, 128);
  put(b, 128 + 8, 1132);
  put(b, 128 + 0x88, 1);
  put(b, 1132, 1);
  put(b, 1136, 91);
  put(b, 20, 1200);
  put(b, 1200, 1240);
  put(b, 1204, 4500);
  std::copy(raw.model.begin(), raw.model.end(), b.begin() + 1240);
  size_t p = 4500;
  auto record = [&](const Bytes &bytes) {
    put(b, p, uint32_t(bytes.size()));
    p += 4;
    std::copy(bytes.begin(), bytes.end(), b.begin() + p);
    p += bytes.size();
  };
  record(raw.skin);
  record(raw.model);
  if (with_mask)
    record(mask());
  put(b, p, 0xffffffff);
  put(b, 24, 7800);
  put(b, 7808, 8000);
  put(b, 7812, 8500);
  p = 8000;
  record(external(91));
  put(b, p, 0xffffffff);
  p = 8500;
  record(Bytes{0, 2, 255});
  put(b, p, 0xffffffff);
  put(b, 32, 9000);
  put(b, 9000, 1);
  put(b, 9004, 9010);
  put(b, 9010, 0xffffffff);
  return b;
}
} // namespace
int main() {
  try {
    auto raw = fixture();
    const auto original_bytes = raw.model;
    auto padded = raw;
    padded.model.insert(padded.model.begin() + 16, 16, 0);
    half(padded.model, 4, 32);
    auto padded_face = initialize_face_runtime(padded, mask());
    auto padded_pose = actor_bind_pose(padded_face.model.actor);
    uint32_t padded_flags = 0;
    tick_face_runtime(padded_face, padded_pose, 91, padded_flags, 0);
    const auto padded_table = 32 + word(padded_face.model.actor.model, 12);
    ck(word(padded_face.model.actor.model, padded_table + 9 * 52 + 20) ==
               0xffffff98u &&
           word(padded_face.model.actor.model, padded_table + 9 * 52 + 24) ==
               104 &&
           (*padded_pose[9].angles)[0] == 65432,
       "Padded MDL header corrupted runtime copies");
    auto face = initialize_face_runtime(raw, mask());
    auto pose = actor_bind_pose(face.model.actor);
    ck(face.model.tooth.parent == 7 && face.model.tongue.parent == 11 &&
           face.model.eyes[0]->parent == 9 && face.model.eyes[1]->parent == 13,
       "Controller fixture layout");
    uint32_t flags = 0x200;
    ck(tick_face_runtime(face, pose, 91, flags, 0),
       "First idle deformation omitted");
    const size_t table = 16 + word(face.model.actor.model, 12);
    ck(word(face.model.actor.model, table + 9 * 52 + 24) == 104 &&
           word(face.model.actor.model, table + 9 * 52 + 20) == 0xffffff98u &&
           (*pose[9].angles)[0] == 65432 && (*pose[9].angles)[1] == 104,
       "Runtime eye yaw was not inverted");
    const auto first = face.model.actor.model;
    ck(!tick_face_runtime(face, pose, 91, flags, 0) && (flags & 0x80),
       "Idle gate missing");
    ck(face.model.actor.model == first && (flags & 0x200),
       "Idle update changed geometry/other flags");
    ck(tick_face_runtime(face, pose, 91, flags, 0x200),
       "Game override omitted");
    ck(!tick_face_runtime(face, pose, 91, flags, 0x200, 0, 2),
       "Non-final loop deformed");
    const std::vector<Bytes> masks{external(91), external(92), external(0)};
    ck(select_face_mask(face.controller, masks, 0, 1), "Shape request missing");
    face.controller.flags |= 1;
    ck(tick_face_runtime(face, pose, 91, flags, 0),
       "Requested animation idle gated");
    near(face.controller.parameters.jaw_translation, 11);
    ck(face.controller.frame == 2 && face.controller.last_frame == 4,
       "Sampling/advance order");
    tick_face_runtime(face, pose, 91, flags, 0);
    near(face.controller.parameters.jaw_translation, 12);
    tick_face_runtime(face, pose, 91, flags, 0);
    near(face.controller.parameters.jaw_translation, 13);
    ck(face.controller.frame == 3 && !(face.controller.flags & 1),
       "Non-loop final-minus-one hold");
    face.controller.flags = 0x20 | 0x10 | 1;
    tick_face_runtime(face, pose, 91, flags, 0);
    near(face.controller.parameters.jaw_translation, 13); // Reset AFTER sample.
    ck(face.controller.frame == 0 && !face.controller.selected &&
           !(face.controller.flags & 0x31) && face.controller.last_frame == 4,
       "Reset state");
    tick_face_runtime(face, pose, 91, flags, 0x200);
    near(face.controller.parameters.jaw_translation, 0);
    select_face_mask(face.controller, masks, 1, 2);
    tick_face_runtime(face, pose, 91, flags, 0);
    ck(!face.controller.selected && face.controller.frame == 0,
       "Wrong actor changed track");
    select_face_mask(face.controller, masks, 2, 99);
    face.controller.flags |= 3;
    tick_face_runtime(face, pose, 91, flags, 0);
    near(face.controller.parameters.jaw_translation, 10);
    ck(face.controller.frame == 1, "Out-of-range start was not clamped");
    face.controller.frame = 3;
    tick_face_runtime(face, pose, 91, flags, 0);
    ck(face.controller.frame == 0 && (face.controller.flags & 1),
       "Loop boundary");
    face.controller.flags |= 0x10;
    tick_face_runtime(face, pose, 91, flags, 0);
    ck(face.controller.frame == 0, "Pause advanced frame");
    face.controller.phase = 0;
    const auto old_model = face.model.actor.model;
    ck(!tick_face_runtime(face, pose, 91, flags, 0) &&
           face.controller.phase == 1 && face.model.actor.model == old_model,
       "Phase zero must only initialize");
    ck(!select_face_mask(face.controller, masks, 100), "Missing mask selected");
    auto traversed = initialize_face_runtime(
        raw, mask(),
        std::make_shared<const std::vector<Bytes>>(
            std::vector<Bytes>{{1, 12, 255}, {0, 2, 255}}));
    auto traversed_pose = actor_bind_pose(traversed.model.actor);
    uint32_t traversed_flags = 0;
    auto traversed_masks = masks;
    for (size_t i = 0; i < 2; ++i)
      flt(traversed_masks.back(), 8 + i * 164 + 132, 30);
    select_face_mask(traversed.controller, traversed_masks, 0);
    ck(!select_face_mask(traversed.controller, traversed_masks, 99, 1),
       "Missing mask return");
    tick_face_runtime(traversed, traversed_pose, 91, traversed_flags, 0);
    near(traversed.controller.parameters.jaw_translation, 30);
    ck(traversed.controller.mask_number == 0 &&
           traversed.controller.start_frame == 1,
       "Failed mask traversal changed numeric ID or starting frame");
    ck(!select_face_lip(traversed.controller, 99) &&
           traversed.controller.lip_number == 0,
       "Failed lip traversal return or numeric ID");
    traversed.controller.flags |= 4;
    tick_face_runtime(traversed, traversed_pose, 91, traversed_flags, 0);
    near(traversed.controller.parameters.jaw_translation, 0);
    ck(traversed.controller.lip.time == 2 &&
           traversed.controller.selected_clip == 1,
       "Failed lip traversal did not retain last visited record");
    ck(raw.model == original_bytes, "Raw model mutated");
    auto clips = std::make_shared<const std::vector<Bytes>>(
        std::vector<Bytes>{{1, 12, 255}, {0, 2, 255}});
    auto talking = initialize_face_runtime(raw, mask(), clips);
    auto talking_pose = actor_bind_pose(talking.model.actor);
    uint32_t talking_flags = 0x80;
    talking.controller.flags = 4;
    tick_face_runtime(talking, talking_pose, 91, talking_flags, 0);
    near(talking.controller.parameters.jaw_translation, 4);
    near(talking.controller.parameters.eye.x, 1);
    ck(!(talking_flags & 0x80) && talking.controller.lip.count == 2,
       "Lip update remained idle or did not start");
    select_face_lip(talking.controller, 1);
    tick_face_runtime(talking, talking_pose, 91, talking_flags, 0);
    near(talking.controller.parameters.jaw_translation, 4);
    ck(talking.controller.lip.count == 4,
       "Clip selection interrupted active lip");
    for (int i = 0; i < 5; ++i)
      tick_face_runtime(talking, talking_pose, 91, talking_flags, 0);
    ck(!talking.controller.lip.active && !(talking.controller.flags & 4),
       "Lip termination did not clear request flag");
    near(talking.controller.parameters.jaw_translation, 0);
    talking.controller.flags |= 4;
    tick_face_runtime(talking, talking_pose, 91, talking_flags, 0);
    ck(talking.controller.lip.active, "Second lip request failed");
    talking.controller.flags &= ~4u;
    tick_face_runtime(talking, talking_pose, 91, talking_flags, 0);
    ck(!talking.controller.lip.active, "Lip stop ignored");
    auto group_mask = mask();
    group_mask.resize(72 + 12 * 164);
    put(group_mask, 0x20, 12);
    for (size_t i = 0; i < 12; ++i) {
      const size_t p = 72 + i * 164;
      half(group_mask, p + 160, uint16_t(i));
      flt(group_mask, p + 136, 1);
      flt(group_mask, p + 140, 1);
      flt(group_mask, p + 144, -100);
      flt(group_mask, p + 156, 1);
    }
    flt(group_mask, 72 + 7 * 164 + 132, 20);
    auto grouped = initialize_face_runtime(raw, group_mask, clips);
    auto grouped_pose = actor_bind_pose(grouped.model.actor);
    uint32_t grouped_flags = 0;
    grouped.controller.lip_group = 1;
    grouped.controller.flags = 4;
    tick_face_runtime(grouped, grouped_pose, 91, grouped_flags, 0);
    near(grouped.controller.parameters.jaw_translation, 20);
    talking.controller.flags |= 4;
    talking.controller.lip_group = 1;
    const auto before_flags = talking_flags;
    const auto before_model = talking.model.actor.model;
    const auto before_count = talking.controller.lip.count;
    bad([&] {
      tick_face_runtime(talking, talking_pose, 91, talking_flags, 0);
    });
    ck(talking_flags == before_flags &&
           talking.model.actor.model == before_model &&
           talking.controller.lip.count == before_count &&
           (talking.controller.flags & 4),
       "Invalid lip binding partially committed");
    auto malformed = masks;
    malformed[0].pop_back();
    const auto prior_pending = face.controller.pending;
    bad([&] { select_face_mask(face.controller, malformed, 0); });
    ck(face.controller.pending == prior_pending,
       "Malformed mask changed request");
    // Sampling succeeds, then the eye calculation fails late. No staged
    // controller counters, actor flags, runtime pose or model bytes may commit.
    auto late = initialize_face_runtime(raw, mask());
    auto late_pose = actor_bind_pose(late.model.actor);
    auto corrupt = external(91);
    for (size_t i = 0; i < 2; ++i)
      for (size_t field : {size_t(136), size_t(140), size_t(144)})
        flt(corrupt, 8 + i * 164 + field, 0);
    select_face_mask(late.controller, {corrupt}, 0);
    uint32_t late_flags = 0x80;
    const auto late_model = late.model.actor.model;
    const auto late_angles = late_pose[9].angles;
    bad([&] { tick_face_runtime(late, late_pose, 91, late_flags, 0); });
    ck(late.controller.flags == 8 && !late.controller.selected &&
           late.controller.frame == 0 && late.controller.idle_count == 0 &&
           late_flags == 0x80 && late.model.actor.model == late_model &&
           late_pose[9].angles == late_angles,
       "Late deformation failure partially committed");
    bad([&] { tick_face_runtime(face, pose, 91, flags, 0, 0, 0); });
    ScenarioState state;
    state.flags[6][0] = 2;
    state.enemies.resize(2);
    state.enemy_slots = {1};
    state.face_masks = std::make_shared<const std::vector<Bytes>>(masks);
    auto &actor = state.enemies[1];
    actor.flags = 1;
    actor.id = 91;
    actor.model = std::make_shared<const ActorModel>(raw);
    actor.face = initialize_face_runtime(raw, mask(), clips);
    actor.joint_pose = actor_bind_pose(actor.face->model.actor);
    actor.model_work = actor.face->model.work;
    Motion motion;
    motion.flags = 0;
    motion.frames = 8;
    motion.tracks.resize(7); // Seven destinations outside facial joints 6..14.
    for (auto &track : motion.tracks)
      track.angles.assign(8, std::array<uint16_t, 3>{});
    motion.tracks[0].positions.assign(8, Vec3{});
    actor.motions = std::make_shared<const MotionBank>(MotionBank{motion});
    run_scenario(Bytes{60, 99, 0, 0, 62, 99, 0, 0, 0, 0}, state);
    ck(actor.face->controller.flags == 9, "Native mask commands missing");
    tick_scenario_actors(state);
    ck(actor.modes[0] == 1 && actor.modes[3] == 3 && actor.frame == 32768 &&
           actor.face->controller.frame == 1 && actor.rendered_model &&
           actor.rendered_model->meshes.size() == 6,
       "ID91 startup did not join mask, motion and rendering");
    near(actor.face->controller.parameters.jaw_translation, 10);
    const auto startup_frame = actor.frame;
    tick_scenario_actors(state);
    ck(actor.frame == startup_frame && actor.face->controller.frame == 2,
       "Subplayer hold lost mask updates or advanced body motion");
    run_scenario(Bytes{61, 0, 0, 0, 63, 99, 0, 0, 0, 0}, state);
    ck(actor.face->controller.flags & 4, "Native lip commands missing");
    tick_scenario_actors(state);
    near(actor.face->controller.parameters.jaw_translation, 4);
    ck(actor.face->controller.lip.count == 2,
       "Native lip overlay did not tick");
    run_scenario(Bytes{62, 0, 0, 1, 63, 0, 0, 1, 0, 0}, state);
    ck(!(actor.face->controller.flags & 5), "Native mask/lip stop ignored");
    run_scenario(Bytes{60, 0, 0, 255, 61, 0, 0, 255, 0, 0}, state);
    ck(actor.face->controller.mask_number == 0 &&
           actor.face->controller.lip_number == 0,
       "Missing resident selection overwrote prior bindings");
    for (uint8_t op : {60, 61, 62, 63}) {
      bad([&] { run_scenario(Bytes{op}, state); });
      bad([&] { run_scenario(Bytes{op, 0, 1, 0, 0, 0}, state); });
    }
    for (int control2 : {0, 10}) {
      auto moving = state;
      moving.flags[11][0] |= 0x200;
      auto &body = moving.enemies[1];
      auto animated = motion;
      animated.tracks[0].positions.assign(8, Vec3{8, 0, 0});
      for (auto &track : animated.tracks)
        track.angles.assign(8, std::array<uint16_t, 3>{2222, 0, 0});
      body.motions = std::make_shared<const MotionBank>(MotionBank{animated});
      body.modes = {1, 0, 0, 0};
      body.control2 = control2;
      body.blend_count = 4;
      body.frame = 0;
      body.joint_pose[6].angles = std::array<uint16_t, 3>{1111, 2222, 3333};
      tick_scenario_actors(moving);
      near(body.joint_pose[0].position->x, control2 == 0 ? 1 : 2);
      ck(body.frame == 32768 && body.modes[3] == 1 &&
             body.blend_count == (control2 == 0 ? 7 : 3) &&
             *body.joint_pose[6].angles ==
                 std::array<uint16_t, 3>{1111, 2222, 3333},
         "Body blend startup overwrote facial motion mask");
      const auto eye = body.joint_pose[9].angles;
      tick_scenario_actors(moving);
      ck(body.frame == 65536 && body.blend_count == (control2 == 0 ? 6 : 2) &&
             body.joint_pose[9].angles == eye &&
             *body.joint_pose[6].angles ==
                 std::array<uint16_t, 3>{1111, 2222, 3333} &&
             body.rendered_model,
         "Body animation lost facial updates or rendering");
    }
    ScenarioState loaded;
    const auto room_bytes = room_fixture(raw);
    bind_room_enemies(room_bytes, RoomCollision{}, loaded);
    ck(loaded.enemies[0].face && loaded.enemies[0].joint_pose.size() == 16 &&
           loaded.enemies[0].model_work && loaded.face_masks &&
           loaded.face_masks->size() == 1 &&
           loaded.enemies[0].face->controller.selected_clip == 0,
       "Finish-room did not bind native facial work and resident data");
    ck(loaded.enemies[0].model->model == raw.model &&
           loaded.enemies[0].model_bank->mask == mask(),
       "Native binding changed raw assets");
    loaded.flags[6][0] = 2;
    loaded.enemies[0].damage.fill(99);
    tick_scenario_actors(loaded);
    ck(loaded.enemies[0].rendered_model && loaded.enemies[0].modes[0] == 1,
       "Bound facial actor failed startup");
    ck(loaded.enemies[0].damage[15] == 0 && loaded.enemies[0].damage[16] == 99,
       "Facial actor damage cleanup did not use expanded model work");
    loaded.enemies[0].state_flags |= 0x20;
    tick_scenario_actors(loaded);
    ck(loaded.enemies[0].secondary_flags == 0x80 &&
           loaded.enemies[0].state_flags == 0x20,
       "Facial idle control confused flg2 with stflg");
    auto no_mask = loaded;
    bind_room_enemies(room_fixture(raw, false), RoomCollision{}, no_mask);
    ck(!no_mask.enemies[0].face && !(no_mask.enemies[0].model_flags & 0x100) &&
           no_mask.enemies[0].model_work &&
           (no_mask.enemies[0].model_work->joints[10].flags & 4),
       "Missing MASK branch did not retain original motion masks");
    std::cout << "Native facial controller timing and runtime copies passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
