# Native facial controller integration plan

> **For agentic workers:** Use superpowers:executing-plans inline with test-first changes.

**Goal:** Integrate original `bhControlMask` timing and runtime joint copies into the selected ID 91 native actor path required by the Step 1 graveyard slice.

**Architecture:** A facial runtime owns prepared model/deformation and a controller with immutable key/clip bindings plus mutable native counters. Stage controller, secondary actor flags, pose and model changes together. Preserve the raw model bank as immutable provenance; render and motion use the prepared facial model through a single scenario helper. External mask tracks and lips use original room resident bank indices and script opcodes 60–63.

**Tech Stack:** C++17, CMake/Ninja, existing native facial/motion/model modules, headless CTest and private retail probe.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`

## Global constraints

- Native Windows x64 code derived from the pinned USA decompilation.
- Keep the pinned upstream checkout unmodified as reference.
- All verification headless; no retail data in Git/package.
- Full Step 1 remains incomplete until connected rooms, interactions, combat, inventory, audio and versioned save/load are demonstrated.
- New Game remains disabled until the full playable slice is verified.

## Review focus

- Shape frame is sampled before advance/reset; finishing holds last minus one.
- Final-loop and idle gates suppress deformation while parameters still update.
- Wrong external actor ID clears the request/idle gate but does not switch tracks.
- Lip start/stop/termination preserve source flag order and unselected eyes/muscles.
- Runtime eye X/Y are inverted after context deformation; failures cannot partially commit model, pose, counters or flags.

## Task 1: Controller and original runtime joint copies

**Files:** Create `src/face_controller.hpp/.cpp`, `tests/face_controller_tests.cpp`; modify `src/face_parameters.hpp/.cpp`, `CMakeLists.txt`.

**Interfaces:** `FaceRuntime initialize_face_runtime(const ActorModel&, const Bytes&, shared_ptr<const vector<Bytes>>)` owns facial resources. `bool tick_face_runtime(FaceRuntime&, vector<JointPose>&, uint16_t actor_id, uint32_t& state_flags, uint32_t game_flags, int loop_now=0, int loop_count=1)` preserves original ordering and selected joint layout 7/11/9/13. `select_face_mask` and `select_face_lip` retain original missing-index return behavior and request fields. `decode_face_parameter_records`/`face_track_records` share bounded ordinary key decoding with external tracks and packed lip input.

- [x] Write tests using a synthetic four-clone hierarchy with original tooth 7/tongue 11/eyes 9,13. Check first idle draw, suppressed second draw, game flag 0x200, non-final loop, phase zero, external shape ID mismatch, start/clamp/reset/loop/pause and sampled-before-increment behavior.
- [x] Observe a failing executable against minimal unavailable-controller implementations before implementing behavior.
- [x] Port `bhControlMask` native state and deform/copy logic. Keep the existing source context matrices separate from copied runtime angle values. Check selected layout before update.
- [x] Add lip overlay, malformed external shape, wrong group, late deformation failure and complete rollback tests; run the targeted controller tests and existing facial tests.

## Task 2: Actor lifecycle and original script commands

**Files:** Modify `src/scenario.hpp/.cpp`, `tools/room_inspect.cpp`, `docs/actor-rendering-status.md`; extend controller/scenario tests.

- [x] Write failing integration tests for original mask/lip set/start commands (60–63), authored work slot mapping, selected ID91 startup, O_WORK motion masks and persistent work rendering.
- [x] Bind selected ID91 facial work at finish-room lifecycle; leave raw model bank immutable and retain explicit failure for unsupported actor states. Bind resident external mask/lip banks. Apply mask controller before the original subplayer callback and retain selected joint flags during motion.
- [x] Exercise real graveyard ID91 startup/hold and facial motion using the production scheduler in isolation from the still-pending zombie controller. Keep full room transition failure until that required controller exists.
- [x] Run `./tools/build.ps1`, complete private retail runtime scan and packaged runtime smoke with fresh ignored profile. Request source review, resolve material findings, document limits and commit.

## Source evidence

`face.c` bhInitMask/bhControlMask/bhSetMask/bhSetLip; `face_bh.c` deformation/lipsync; `room.c` bhFinishRoom; `eneset.c` bhControlEnemy; `subpl.c` bhSubpl/em60_init; `event.c` opcodes 60–63 and flag bank 11 = gm_flg.
