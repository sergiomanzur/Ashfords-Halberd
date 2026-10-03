#include "runtime.hpp"
#include "zombie.hpp"
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void rejects(F f, const char *text) {
  try {
    f();
  } catch (const std::runtime_error &e) {
    ck(std::string(e.what()).find(text) != std::string::npos, e.what());
    return;
  }
  throw std::runtime_error("Missing explicit controller boundary");
}
ScenarioState startup_fixture() {
  ScenarioState state;
  state.enemies.resize(128);
  state.collision = RoomCollision{};
  state.flags[6][0] = 2;
  ActorModel model;
  model.model.resize(16 + 18 * 52);
  auto put = [&](size_t p, uint32_t v) {
    for (size_t i = 0; i < 4; ++i)
      model.model[p + i] = uint8_t(v >> (8 * i));
  };
  model.model[0] = 'M';
  model.model[1] = 'D';
  model.model[2] = 'L';
  model.model[3] = 128;
  model.model[4] = 16;
  model.model[6] = 18;
  put(8, 0xffffffff);
  for (size_t joint = 0; joint < 18; ++joint) {
    const size_t p = 16 + joint * 52;
    put(p, 6);
    put(p + 4, 0xffffffff);
    if (joint)
      put(p + 12, 0x3f800000); // Unit Y bind links.
    put(p + 44, joint == 17 ? 0xffffffff : uint32_t((joint + 1) * 52));
    put(p + 48, 0xffffffff);
  }
  auto bank = std::make_shared<ActorModelBank>();
  bank->models = {model, model};
  auto motions = std::make_shared<MotionBank>(512);
  for (uint32_t number : {2u, 202u}) {
    Motion motion;
    motion.frames = 3;
    motion.flags = 0x10;
    motion.tracks.resize(number == 2 ? 18 : 10);
    for (auto &track : motion.tracks)
      track.angles.assign(3, std::array<uint16_t, 3>{});
    motion.tracks[0].positions.assign(3, number == 2 ? Vec3{2, 0, 0}
                                                     : Vec3{0, 4, 0});
    (*motions)[number] = motion;
  }
  for (size_t slot : {0u, 2u}) {
    auto &root = state.enemies[slot];
    root.flags = 1;
    root.id = 1;
    root.model_version = slot ? 32 : 9;
    root.model_bank = bank;
    root.model = std::shared_ptr<const ActorModel>(bank, &bank->models[0]);
    root.motions = motions;
    root.position = {100, 200, 300};
  }
  return state;
}
ScenarioEntity player_fixture() {
  ScenarioEntity player;
  player.flags = 0x11c;
  player.position = {1000, 200, 1000};
  player.previous_position = player.position;
  player.geometry.radius = 3.5f;
  player.geometry.height = 16.5f;
  player.geometry.collision_radius = 2.5f;
  player.geometry.collision_height = 15.5f;
  player.player_work.emplace();
  player.modes = {1, 0, 0, 0};
  return player;
}
int main() {
  try {
    ScenarioState skip;
    skip.flags[11][0] = 0x81234567;
    run_scenario(Bytes{106, 0, 0}, skip);
    ck(skip.flags[11][0] == 0xc1234567, "Original EventSkipSet enable wrong");
    run_scenario(Bytes{106, 2, 0}, skip);
    ck(skip.flags[11][0] == 0x81234567, "Original EventSkipSet disable wrong");
    rejects([&] { run_scenario(Bytes{106}, skip); }, "Truncated");
    Bytes script_room(1300);
    auto put_script = [&](size_t offset, uint32_t value) {
      for (size_t i = 0; i < 4; ++i)
        script_room[offset + i] = uint8_t(value >> (i * 8));
    };
    put_script(0, 0x41200000);
    put_script(16, 128);
    put_script(28, 1200);
    put_script(32, 1250);
    put_script(1200, 16);
    put_script(1204, 32);
    put_script(1208, 34);
    put_script(1212, 34);
    const Bytes request{51, 0, 0x80, 0, 0, 2, 0, 1, 0};
    for (size_t i = 0; i < request.size(); ++i)
      script_room[1216 + i] = request[i];
    ScenarioState scripted;
    scripted.scenario =
        std::make_shared<const RoomScenario>(decode_room_scenario(script_room));
    auto scripted_player = player_fixture();
    initialize_scenario(scripted, &scripted_player);
    ck(scripted.player_state_flags == 0x80010000 &&
           scripted_player.state_flags == 0x80010000,
       "Initialization script player flags lost across runtime work bridge");
    auto state = startup_fixture();
    auto player = player_fixture();
    tick_scenario_actors(state, &player);
    ck(!(player.flags & 4), "Source Init did not clear player damage marker");
    for (size_t root : {0u, 2u}) {
      const auto &e = state.enemies[root];
      ck(e.zombie_work && e.zombie_work->body,
         "Root startup missing linked body");
      const auto &b = state.enemies[*e.zombie_work->body];
      ck(e.modes[0] == 1 && b.modes[0] == 1 && b.zombie_work,
         "Same-pass root/body startup ordering missing");
      ck(e.frame == 2 * 65536 && b.frame == 2 * 65536,
         "Original Init plus MainLoop SetMtn sampling missing");
      ck(b.pose_owner == root && b.linked_parent == root,
         "Original shared pose owner changed");
      ck(e.model_matrix && e.geometry.capsule_radius == 2,
         "Linked-body post-collision parent model/capsule update missing");
    }
    for (const auto &e : state.enemies)
      if ((e.flags & 1) &&
          (e.id == 31 || e.id == 32 || e.id == 33 || e.id == 38))
        ck(e.modes[0] == 1,
           "Allocated auxiliary was not scheduled in same pass");
    rejects([&] { tick_scenario_actors(state, &player); },
            "Unimplemented zombie runtime mode 1");
    auto hidden = state;
    for (auto &e : hidden.enemies)
      if (e.id == 1 && !(e.flags & 0x80))
        e.state_flags |= 0x1000000;
    tick_scenario_actors(hidden);
    ck(hidden.enemies[0].frame == state.enemies[0].frame &&
           hidden.enemies[*hidden.enemies[0].zombie_work->body].frame ==
               state.enemies[*state.enemies[0].zombie_work->body].frame,
       "Hidden parent gate ran root or linked-body controller");
    auto missing = startup_fixture();
    rejects([&] { tick_scenario_actors(missing); },
            "Missing scheduled zombie player work");
    ck(!missing.enemies[0].zombie_work,
       "Missing-player boundary allocated work");
    auto gated = startup_fixture();
    gated.flags[6][0] = 0;
    tick_scenario_actors(gated);
    ck(!gated.enemies[0].zombie_work, "Disabled subsystem ran startup");
    gated.flags[6][0] = 2;
    for (auto &e : gated.enemies)
      if (e.flags & 1)
        e.state_flags = 0x40000000;
    tick_scenario_actors(gated);
    ck(!gated.enemies[0].zombie_work, "Frozen enemy ran startup");
    auto held = startup_fixture();
    for (auto &e : held.enemies)
      if (e.flags & 1)
        e.state_flags = 0x40000000;
    initialize_zombie_work(held, 0);
    auto &root = held.enemies[0];
    root.state_flags = 0;
    root.modes[0] = 6;
    root.zombie_flags |= 0x10;
    root.zombie_flags &= ~0xfu;
    root.zombie_work->attack_delay = -1;
    root.zombie_work->frame_timer_30 = 2;
    root.zombie_work->frame_timer_34 = -1;
    root.combo_timeout = -1;
    root.combo_flags |= 0x70;
    root.secondary_flags |= 4;
    root.damage[17] = 3;
    root.damage[18] = 5;
    tick_scenario_actors(held, &player);
    const auto &after = held.enemies[0];
    ck(after.zombie_work->attack_delay == -2 &&
           after.zombie_work->frame_timer_30 == 1 &&
           after.zombie_work->frame_timer_34 == -1,
       "Original unsigned byte/positive word countdown wrong");
    ck(after.combo_timeout == -2 && !(after.combo_flags & 0x70) &&
           !(after.secondary_flags & 4) && after.damage[17] == 0 &&
           after.damage[18] == 5,
       "Successful controller source damage cleanup wrong");
    auto hit = startup_fixture();
    hit.enemies[0].flags |= 4;
    rejects([&] { tick_scenario_actors(hit, &player); },
            "Unimplemented zombie runtime damage check");
    ck(!hit.enemies[0].zombie_work, "Unsupported hit silently ran Init");
    RoomRuntime runtime;
    const auto &model = state.enemies[0].model;
    PlayerPackage package;
    package.models.push_back(*model);
    package.motions.resize(43);
    package.motions[42] = *state.enemies[0].motions->at(2);
    runtime.world.player.position = {3, 4, 5};
    runtime.world.player.yaw = 16384;
    runtime.world.player.frame = 65536;
    runtime.world.player.motion = 42;
    runtime.world.floor = 1;
    runtime.scenario.player_work_flags = 0x118;
    runtime.scenario.player_secondary_flags = 0x200;
    runtime.scenario.player_health = 123;
    runtime.player_pose = actor_bind_pose(*model);
    synchronize_runtime_player(runtime, package);
    const auto &pw = runtime.player_work;
    ck(pw.position.x == 3 && pw.position.y == 4 && pw.position.z == 5 &&
           pw.angles[1] == 16384 && pw.floor == 1 && pw.health == 123 &&
           pw.flags == 0x118 && pw.secondary_flags == 0x200 &&
           pw.frame == 65536 && pw.motion_number == 42 && pw.modes[0] == 1,
       "Native runtime player scalars missing");
    ck(pw.geometry.radius == 3.5f && pw.geometry.height == 16.5f &&
           pw.geometry.collision_radius == 2.5f &&
           pw.geometry.collision_height == 15.5f,
       "Player work differs from original PlyInfo");
    ck(pw.model && pw.base_motions && pw.motions == pw.base_motions &&
           pw.model_work && pw.model_work->joints.size() == 18 &&
           (pw.model_work->joints[7].flags & 8) &&
           (pw.model_work->joints[11].flags & 8),
       "Native player pose/cache/motion bank missing");
    ck(pw.player_work && pw.player_work->status_flags == 1 &&
           pw.player_work->normal_anchor.x == 0,
       "Player EXP1 startup or separate normal-anchor lifetime wrong");
    const auto bound = pw.model;
    const auto motions = pw.base_motions;
    runtime.player_work.player_work->normal_anchor = {7, 8, 9};
    runtime.world.player.position.x = 6;
    synchronize_runtime_player(runtime, package);
    ck(runtime.player_work.model == bound &&
           runtime.player_work.base_motions == motions &&
           runtime.player_work.player_work->normal_anchor.x == 7 &&
           runtime.player_work.model_work->base_matrix[3] == 6,
       "Runtime binding replaced retained resources/normal anchor or missed "
       "cache update");
    std::cout << "Native zombie runtime startup passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
