# Native zombie awareness and room routing implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline, test-first, followed by one source review.

**Goal:** Port the original awareness counter and route data/queries needed by zombie Brain00 and movement.

**Architecture:** Immutable room route regions and a destination-row/source-column next-hop matrix bind during native room loading and replace previous-room data transactionally. Awareness updates use cached neck matrices, signed object yaw, retained personality index and source packed bytes, staging only changed scalar fields.

**Tech Stack:** C++17, CMake/Ninja, synthetic and private retail headless checks.

**Spec:** `docs/superpowers/specs/2026-10-02-windows-port-design.md`

## Global constraints

- Native Windows x64 from pinned MIT source; private assets remain immutable and uncommitted.
- All verification headless, fresh ignored profiles, no normal user profile changes.
- Route/awareness prerequisites do not complete root movement, combat or Step1; New Game remains disabled.

## Review focus

- Route region membership ignores ATR flags and includes all three axes and both endpoints; first matching index wins.
- Matrix rows index destination, columns source. Same-region query copies the destination but returns0xff; missing region leaves output untouched.
- Zero-route rooms do not consume unused pointers. Diagonal0xff entries are legitimate because source never indexes them; an invalid selected hop rejects before output mutation.
- Awareness uses the retained EXP0 personality and signed neck yaw, not a recomputed model personality or body parent yaw.
- USA ELF17a504..17a514 decrements/stores the timeout, masks0xff and branches bgez: notice flag clearing is unreachable. Preserve this observed behavior rather than making a signed timer fix.

## Task1: Room routes

**Files:** `src/room.hpp`, new `src/room_routes.cpp`, `src/scenario.hpp`, `src/runtime.cpp`, new `tests/room_routes_tests.cpp`, `tests/room_tests.cpp`, `CMakeLists.txt`.

**Interfaces:** `RoomRoutes {vector<CollisionRecord> regions; Bytes next_hops;}`; `decode_room_routes(Bytes)`; `uint8_t room_route_id(RoomRoutes,Vec3)`; `uint8_t room_route(RoomRoutes,Vec3,Vec3,Vec3& output)`; immutable `ScenarioState::routes`.

- [x] Test decoder offsets0x28/0x2c/count0xa8, finite bounded36-byte ATR records, route count<=255, full count-squared byte matrix, missing/truncated pointer rejection, zero-route pointer suppression and unused diagonal0xff.
- [x] Test inactive route flags, first overlapping region, XYZ inclusive boundaries, matrix orientation/center, same-region output, missing-region preservation and selected invalid-hop rollback. Observe explicit pending stub failure before implementation.
- [x] Bind replacement routes in load_room_runtime; test prior-pointer replacement and failed-transition preservation against the production loader, then pass targeted tests.

## Task2: Native awareness

**Files:** `src/zombie.hpp/.cpp`, new `src/zombie_awareness.cpp`, `src/scenario.hpp`, new `tests/zombie_awareness_tests.cpp`, `tools/room_inspect.cpp`, `docs/scenario-status.md`, `CMakeLists.txt`.

**Interfaces:** retained `ZombieExtendedWork::awareness`/`awareness_timeout` bytes; `zombie_personality_at(uint8_t)`; `tick_zombie_awareness(ScenarioState&,size_t,Vec3 player)`.

- [x] Pin first detection,31-frame counter rollover, latched view bit, last-frame loss of view, unsigned timer wrap with persistent notice flag, reserved bits and skipped low31 query. Test retained personality, source probeY0, signed neck yaw, alias ownership, invalid consumed context and scalar rollback; observe pending stub failure.
- [x] Implement source branch order using native targeting query. Confirm initialization/reinitialization resets the new bytes through existing extended-work construction.
- [x] Decode every retail route table and exercise actual route centers/matrix hops; run isolated graveyard awareness traces using actual initialized cached neck work and entry player position, explicitly separating these from full root AI frames.
- [x] Run full build/CTest/package, fresh private retail scan and packaged headless smoke; source review, diff/format checks, document limits and commit.

## Source evidence

`rutchk.c::bhCheckRoute`, `bhCheckRouteID`; `types.h::ROM_WORK`; `en01.c::bhEne01_EneSearch`; private USA ELF0x17a3a0..0x17a540, specifically unsigned-byte timeout branch0x17a504..0x17a514. Decompiled source and retail machine code agree.

## Verification result

Completed source review and targeted tests; private retail scan passed all 205 rooms, 2,688 route regions, 85,944 valid route probes and 1,860 isolated awareness updates. The 19,556 out-of-range next-hop probes remain excluded. Full Windows build passed all 29 CTests, installed the executable and rebuilt the ZIP. A fresh packaged headless runtime profile passed 241 ticks, 240 rendered frames, 53 movement ticks, ten focus-blocked ticks and main-menu return with INI persistence. Diff and production/new-test formatting checks passed.

The first full run failed in the existing core-boundaries interrupted-cache directory rename with Permission denied. Inspection found the previous directory intact and no live directory. An unchanged focused rerun and subsequent full run both passed; the cause remains unidentified, so this is recorded as a non-reproduced filesystem failure without an unverified fix. This increment does not enable full zombie root AI or complete Step 1.
