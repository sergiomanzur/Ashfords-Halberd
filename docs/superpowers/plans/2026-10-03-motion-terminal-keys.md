# Source terminal motion key implementation plan

> Execute inline with superpowers:executing-plans and test-driven-development.
> One final fresh read-only source review before the verification commit.

**Goal:** Safely sample the terminal frame reached by native walking transitions,
then advance/wrap the counter in the original order.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Architecture:** Normalize the sampling index at the terminal frame to frame0,
keeping the literal work frame and post-sampling counter/boundary update intact.
Use the same bounded attribute lookup. No key count or decoded asset changes.
**Technology:** C++17/CMake/CTest, pinned Motion.c/binfunc.c, private USA assets.

## Constraints and source decisions

- Headless only; preserve default profile and retail assets.
- Source SetMtnNormal/Fast/Slow reads frame/65536 directly before bhSetMotion
  advances it; Slow wraps only the interpolation destination. FrameNum's clamp
  and caller shift remain unchanged.
- Retail RM0020/0021 terminal channels are inside their own record, but the last
  angle overlaps the relocated MDATA pointer table. binfunc.c rewrites those
  words to addresses; raw disk reads would therefore not reproduce runtime keys.
- Native compatibility fix: sample terminal as the start of the next loop,
  never interpreting pointer words as angle keys. This is a deliberate fix to
  original out-of-range channel reads, not a claim of identical terminal poses.
- Admit one additional source frame/fraction window, not arbitrary out-of-range
  indices. No extra frame count and no source counter timing change.
- Root pursuit scheduling/combat/effect playback remains pending. New Game
  stays disabled and full Step1 remains active.

## Task1: terminal sampling and attribute reads

Files: src/motion.hpp/cpp, src/zombie.cpp, src/scenario.cpp,
tests/motion_terminal_tests.cpp, CMakeLists.txt.

- [x] Add a named failing check using a synthetic bounded motion with distinct
  adjacent bytes. RED must be the existing invalid-application-frame rejection.
- [x] Normalize sampling for Fast/Normal/Slow and Hokan, including mirrored
  channels and fractional frame0-to-frame1 interpolation. Read normalized
  attributes before advancement without changing authored metadata.
- [x] Cover truncated records, ordinary last-frame
  interpolation, unchanged preview looping, counter overflow and rollback.

## Task2: retail integration and verification

Files: tools/room_inspect.cpp, docs/player-motion-status.md,
docs/scenario-status.md, this plan.

- [x] Exercise paired native SetMtn/CalcEnemy at all10retail remap boundaries;
  compare independently sampled frame0 and verify post-sampling
  counter boundary/body pose and finite cache/capsule. Foot-fix callbacks retain
  the original incoming frame, so world translations can differ from frame0.
  Retain existing205-room probes.
- [x] Request final read-only source review; fix material findings RED-to-GREEN.
  Run full headless build, fresh packaged dummy-SDL smoke, retail scan and
  formatting/diff checks. Record evidence and commit.

Verified:49/49 headless checks,205retail rooms and10paired terminal sampling/
cache boundaries. Fresh packaged dummy-SDL smoke:241ticks,240renders,
53movement ticks,10focus-blocked ticks, menu return and INI creation.
Review's Important diagnostic comparator finding had retail RED-to-GREEN;
no remaining material production findings. Deferred Minors: mirrored Hokan
regression coverage and scenario actors' stricter caller frame guard.
