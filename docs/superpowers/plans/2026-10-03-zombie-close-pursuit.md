# Native zombie close pursuit implementation plan

> Execute inline using superpowers:executing-plans and test-driven-development.
> Request one final read-only source review before committing this increment.

**Goal:** Port complete MV02 close-pursuit state and its ChgWalkMtn/FastWalkCheck
dependencies toward the original native graveyard encounter.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Architecture:** Stage only enemy/body motion scalars, EXP94/98 words, signed
attack-delay byte, yaw, timers, RNG and sound requests. Do not clone assets or
poses per tick. Commit after consumed context validates.
**Technology:** C++17, CMake/CTest, pinned MIT source, existing yaw/wall helpers.

## Source rulings and limits

- MV02 is close pursuit, not the grab itself. Preserve all three submodes,
  branch-specific RNG, startup fallthrough, the two distinct distance windows,
  notice/boundary gates, transition timer tail and timed voice requests.
- EXP94 is reset through a float zero store and then read/written as an integer
  word. Store its exact word, not a float value1. EXP98 is a signed timer.
- ChgWalkMtn reads the body even before its later null gate. Require a valid
  body for this helper instead of inventing a fallback animation.
- Preserve bhGetFrameNum's existing raw-count clamp and the caller's second
  fixed-point shift. USA ELF confirms the call at0x186798 and shift at0x1867a8.
  Integer arithmetic uses source low32-bit wrap; float conversion is guarded.
  These literal phase changes may reach the clip boundary; terminal-key handling
  belongs to subsequent motion integration and must be resolved before the full
  controller/playable slice is enabled.
- FastWalkCheck scans active source roots; skipped works and type5 do not block.
- Retain source sound requests; device playback, complete root Move/actions,
  grabs, weapon/damage/death controllers and effect playback remain pending.
  Keep the full Step1 goal active and New Game disabled.

## Task1: literal source pursuit implementation

Files: scenario.hpp, zombie.hpp, new zombie_pursuit.cpp,
new tests/zombie_pursuit_tests.cpp, CMakeLists.txt.

- [x] Add interfaces/state fields and failing tests for source startup, linked
  pairs, nearby acceleration, exact timers/RNG, transition tail and legacy phase.
  Expected RED: pending pursuit/changer functions fail named behavior tests.
- [x] Implement all MV02 branches, ChgWalkMtn paired selection/phase/blends and
  FastWalkCheck filters. Test short-circuit RNG gates, strict cone and distance
  edges, type5 behavior, steering windows, rollback and ignored unused inputs.
  Expected GREEN: new focused checks pass without warnings.

## Task2: retail verification and source review

Files: tools/room_inspect.cpp, docs/scenario-status.md, this plan.

- [x] Add actual retail cached-model pursuit state probes with paired authored
  motion banks and original room walls. Preserve205rooms/1600SetMtn/CalcEnemy,
  3200effect routers/20effect-birth probes/4800foot paths/30mouth draws.
- [x] Perform final read-only source review against en01.c MV02/ChgWalkMtn/
  FastWalkCheck, pwksub.c FrameNum and source types. Fix material findings with
  RED-to-GREEN regressions; run full tools/build.ps1 and fresh packaged dummy-SDL
  smoke/retail scan/format/diff checks. Record evidence and commit progress.

Verified:48/48 headless checks,205retail rooms,180isolated pursuit ticks and
10literal phase boundaries. Fresh packaged dummy-SDL smoke:241ticks,
240renders,53movement ticks,10focus-blocked ticks, menu return and INI creation.
Source review: no Critical/Important findings. Deferred Minor: add a nonzero
same-motion frame/blend retention regression during terminal-key integration.
