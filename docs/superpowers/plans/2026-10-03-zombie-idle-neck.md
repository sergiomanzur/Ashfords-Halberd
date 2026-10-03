# Native zombie idle and linked-body neck implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline, test-first, with one source review.

**Goal:** Port original MV00 idle timing/motion selection and SearchNeck pose handling required by the graveyard root/body update sequence.

**Architecture:** Explicit source ct0–ct3 fields correct the inconsistent prop mapping. Stage scalar changes, original RNG order and source sound requests. SearchNeck resolves stable pose ownership and stages torso/neck yaws without copying geometry.

**Tech Stack:** C++17, CMake/Ninja, headless synthetic/private retail diagnostics.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`

## Global constraints

- Native Windows x64 from pinned MIT source; retail files private and immutable.
- All checks headless with ignored profiles, no desktop or normal settings writes.
- Full movement, action/combat, motion effects, decoded audio and cross-room saves remain required. Step1/New Game incomplete.

## Review focus

- Prop oscillation uses ct0; pickup highlight uses ct0/ct1/ct2. Existing event/subplayer ct1/ct2 stay intact.
- ChgMtn always sets step65536. Unchanged motion returns-1 with other fields preserved; changed resets frame/blend/mode and clears only40000/2000000.
- MV00 random frame uses incoming motion count. State0 falls through into countdown1; type8 retains idle. Sound countdown runs after states2/3 as well.
- Neck oscillation retains signed word wrap/unsigned16 thresholds. Invalid consumed contexts roll back state/RNG.
- SearchNeck bypasses roots and mode>=5; linked union flags/awareness, strict distance35, parent yaw and910 step; override1000 uses short wax and retained signed neck.

## Task 1: Counter representation

**Files:** `src/scenario.hpp`, `src/props.cpp`, `tests/scenario_tests.cpp`.

- [x] Add control0 (ct0), keeping control1–3 as ct1–3. Test independent prop ct0/ct1 and pickup0/1/2 with untouched ct3. Observe RED before remapping props, then GREEN.

## Task 2: Idle controller

**Files:** new `src/zombie_idle.cpp`, `src/zombie.hpp`, `src/scenario.hpp`, new `tests/zombie_idle_tests.cpp`, `CMakeLists.txt`, `src/runtime.cpp`, `tests/room_tests.cpp`.

**Interfaces:**

```cpp
int change_enemy_motion(ScenarioEntity&,uint32_t,int32_t,int32_t);
std::optional<int32_t> choose_zombie_sound(const ScenarioEntity&,uint32_t& seed,int32_t code,bool already_playing);
void tick_zombie_idle(ScenarioState&,size_t);
// EnemySoundRequest {work,position,code}; native queued request, not decoded playback.
```

- [x] Write stub RED fixture with literal seed/frame/counters. Cover changed/unchanged motion, body202, source flags, fallthrough, timer/notice/type8, signed overflow, oscillation caps, mode3 outside switch and sound suppression/personality/status/RNG gate. Reject missing body/motion/frame/personality without partial state.
- [x] Implement ChgMtn/SePlay/MV00 with staged scalars; reserve request storage before commit. Pin production room-loader request clearing RED/GREEN.

## Task 3: SearchNeck

**Files:** new `src/zombie_neck.cpp`, `src/zombie.hpp`, `src/scenario.hpp`, new `tests/zombie_neck_tests.cpp`, `tests/zombie_tests.cpp`, `tools/room_inspect.cpp`, `docs/scenario-status.md`, `CMakeLists.txt`.

**Interface:** `void tick_zombie_neck(ScenarioState&,size_t,const ActorModelWork& player)`.

- [x] Pin no-op gates, parent/body1000 override, short wax, signed neck, union800 and awareness40, strict35, parent yaw,910 step, decay/clamps and invalid parent/owner/joint/player rollback. Observe RED.
- [x] Implement staged source pose/neck changes; preserve cached matrices. Test initializer reset of retained neck.
- [x] Run isolated retail MV00 transitions with real motions/RNG and SearchNeck traces with cached Claire work. Targeted tests, fresh source review, full Windows suite/install/ZIP and packaged headless smoke; diff/format and commit.

## Source evidence

`en01.c::MV00/SearchNeck/SePlay`, `zonzon.c::bhEne_ChgMtn`, `objitm.c::bhObjAct03` and item highlight, event ct1/ct2 writes. These controller calls remain separate from full MainLoop until its pending effects/actions are represented.

## Verification result

Observed RED for pending idle/neck functions and corrected prop counter tests;
then GREEN for the five targeted room/zombie/idle/neck/scenario tests. Native
room request-clearing regression failed before the loader clear and passed
afterward; initializer neck reset passed through existing extended-work reset.
The parent-yaw test expectation was corrected from source arithmetic: relative
angle49152 with neck0 takes negative910, giving64626; no production fix was
needed for that expectation. Neck decay refactoring preserved its green tests.

Retail headless scan passed205 rooms, including1,350 isolated MV00 updates,
ten idle-to-walk transitions, ten native sound requests and640 SearchNeck
updates. Actual selected idle motion channels were sampled once per root/body
without representing that as full SetMtn effects/fix-offset handling. Source
review approved without findings. Full tools/build.ps1 passed33/33 CTests,
installed the executable and rebuilt the Windows ZIP. Fresh packaged headless
profile passed241 ticks,240 rendered frames,53 movement ticks, ten focus-blocked
ticks, main-menu return and INI persistence. Diff/format checks passed.

Step1 remains active; root movement/action/combat, complete motion effects,
decoded audio, inventory and versioned cross-room saves remain required.
