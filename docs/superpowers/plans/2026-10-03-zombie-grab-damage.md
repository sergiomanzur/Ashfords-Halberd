# Native zombie grab damage and release plan

> Execute inline with executing-plans/TDD; one final fresh read-only review.

**Goal:** Port complete en01.c::NG00 with native wall/cliff/cap dependencies,
then exercise its bite, escape and fatal paths with PlyDG00 and retail motions.
**Architecture:** Native source line collision returns clipped endpoint, last
hit index and plane normal. NG00 stages scalar root/body/player/cap state,
motion selections, shared EXP64 scratch and sound requests; pose caches stay
caller owned and immutable until their normal motion/model update.
**Tech Stack:** C++17/CMake/CTest, pinned MIT hitchkl/NaColi/en01/zonzon source.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.

## Constraints and decisions

- All checks headless, fresh ignored profiles, no changes to private retail data.
- Continue existing authorized codex/windows-foundation checkout.
- Line query retains static-then-dynamic order, fixed initial direction,
  mutated endpoint per hit, face order, raw signed dimensions, height fallback,
  one-sided polygon intersections, strict endpoint exclusion, .025 near-parallel
  fallback and source negative-X type7 conditional.
- Type2/3 top source queries uninitialized eight-point data inside its fill
  loop. Native compatibility fills all eight vertices before the one top query;
  preserve side order and top winding, eliminate undefined stack reads.
- NG00 is the complete held/lunge-grab enemy callback, including every authored
  submode0..9 (7/no matching case means no action). Full Nage routing/NG01/NG02,
  PlayerLink and full root scheduler remain required subsequent work.
- LeverCheck uses pressed pad_ps masks0xf and0x2d00, not held input pad_on.
- Cap child is EXP0 pointer0x14, native children[5]; flight metadata preserves
  signed16 way/wax/waz/waxp and damage_position.x. No child means no cap work.
- New Game remains gated; inventory, audio playback, saves and full combat
  remain required to complete Step1.

## Review focus

- Clip changes do not silently erase counters on same-motion source retention.
- Bite health0 survives; only health<0 marks fatal state; simultaneous escape
  and bite events use source damage-before-timer order.
- Wall queries and late cap/body/cache errors roll back all scalar and queue
  changes; unused submodes do not consume missing context.
- Original geometry includes triangles, slopes, octagonal columns and raw type7
  heights; query order and strict endpoints must survive adaptation.
- Retail checks must join enemy NG00 to player PlyDG00 and sample both banks,
  with recorded limits for the separate PlayerLink positioning dependency.

## Task1: source line collision

Files: src/collision_line.cpp, src/collision.hpp,
tests/collision_line_tests.cpp, CMakeLists.txt.
Interface: `LineCollisionResult query_collision_line(const RoomCollision&,
const std::vector<CollisionRecord>&,Vec3 from,Vec3 to)`.

- [x] Write synthetic geometry checks with literal endpoint/normal results:
  rectangle faces and top/bottom, strict endpoint, closest ordered static/dynamic
  records, one-sided triangle/slope, column sides/top, raw type7 and flags.
- [x] Register pending query, build and run collision-line test.
  Expected: FAIL with native line collision pending.
- [x] Implement full source shape switch and plane arithmetic; rerun.
  Expected: PASS with retained float arithmetic/order and bounded native inputs.

## Task2: complete enemy grab callback

Files: src/zombie_grab.cpp, src/zombie.hpp, src/scenario.hpp,
tests/zombie_grab_tests.cpp, CMakeLists.txt.
Interface: `tick_zombie_grab(ScenarioState&,size_t,ScenarioEntity&,
const std::vector<int32_t>& playing={})` implements NG00 directly.

- [x] Write original-number fixtures for startup/paired character selection,
  frame20 timer, frame25/60 bites, pressed-input escape, exact HP death boundary,
  root/body recovery, wall/cliff correction, cap flight and rollback.
- [x] Register pending controller; targeted zombie-grab-damage test.
  Expected: FAIL with native zombie grab pending.
- [x] Implement all NG00 branches, Zulzul/Gakeoti/FlyingCap dependencies using
  native source services and scalar transaction; rerun.
  Expected: PASS, including source same-motion retention, unused-case gates.

## Task3: joined retail sequence and final gates

Files: tools/room_inspect.cpp, docs/scenario-status.md, this plan.

- [x] Extend original lunge capture probes into NG00 frame25 bite, input escape,
  and fatal release; dispatch PlyDG00 after NG00, then source motion/cache work.
  Expected:10roots with original enemy/player banks; actual health and state
  transitions, retaining all205-room probes.
- [x] One final read-only source review; material findings fixed RED-to-GREEN.
- [x] tools/build.ps1: Expected all tests pass and refreshed executable/ZIP.
  Full original205-room runtime scan and fresh dummy-SDL packaged smoke:
  Expected241ticks/240renders/53moves/10focus-blocked/menu/INI.
- [x] Format/diff checks, evidence/rulings ledger, documentation and commit.

Verified:55/55tests;205retail rooms;3890joined grab states/40bites/20escapes/
20fatal releases; fresh packaged smoke241ticks/240renders/53moves/10blocked
ticks/menu return/INI. Initial asset-recovery rename permission failure is
recorded; independent and full reruns pass. Review has no material findings;
deferred Minor boundary and late-sound-rollback coverage is recorded in the
status document and ledger. This increment does not complete Step1.
