# Native PlayerLink movement tables and step boundaries plan

> Execute inline with superpowers:executing-plans/test-driven-development.

**Goal:** Supply complete original PlayerLink character XZ tables and source
Dansa step-area/temporary-boundary services needed by its collision wrapper.
**Architecture:** Public MIT character tables live in a small constexpr header,
with a checked native lookup and independent pinned-source bit verifier. Pure
collision services tag step state, retain merged static/dynamic area indices,
and produce temporary walls in source north/east/south/west order. WallEx will
consume these before PlayerLink integration; these services do not change Y.
**Tech stack:** Windows x64 C++17/CMake/CTest/Python source verifier, pinned MIT.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Base:**2bb3f9e clean. Prior goal turn is verified progress: complete NG02,
59checks/205rooms/40joined deaths/releases, refreshed EXE/ZIP/fresh smoke.

## Constraints and review focus

- Headless only, private retail assets, native code and isolated profiles.
- Keep full Step1 objective active; callback prerequisites do not prove actual
  PlayerLink, full dispatcher/combat/audio/inventory/save acceptance.
- Preserve all12source tables: Claire/Chris/Steve clips1/2 (60XZpairs each),
  clips4/5 (50pairs each); player_id3 reuses Chris, offsets400/425/450/425.
- Lookup rejects unsupported character/clip/negative or out-of-range frames;
  original uninitialized table index/out-of-bounds is not reproducible behavior.
- Dansa uses inclusive XZ, matching signed floor, active/type2/prm0!=0, first
  static then dynamic. Y and unrelated geometry/flags are not consumed.
- CheckDansa clears20000 every call, sets it only with matching area and none
  of state bits30. Limit creation uses actual work position/radius, retains the
  selected area, falls back to the old area only if no current area matches.
- Each cardinal probe may be covered by any matching area. Generate only
  uncovered sides, exact source float arithmetic, type1/attr800000/height1.
  No remembered area clears20000. Invalid consumed remembered index or
  nonfinite consumed geometry fails before committing external state.
- Returned temporary walls are caller-owned; the later WallEx transaction must
  append/use/remove them in source order without leaving them as permanent walls.

## Task1: native authored XZ tables

Files: new src/zombie_player_link_tables.hpp,src/zombie.hpp,
new src/zombie_player_link.cpp,new tests/zombie_player_link_tests.cpp,
new tests/zombie_player_link_table_data.py,CMakeLists.txt.
Produces `Vec3 zombie_player_link_offset(uint8_t player_id,
uint32_t motion_number,int32_t frame);` Y is0.

- [x] Add pending lookup and literal fixtures for all4characters/all4clips,
  frame0/interior/last, player3Chris reuse, invalid character/clip/frame.
  ```cpp
  ck(zombie_player_link_offset(0,401,0).z==-4.400000034365803e-05f,
     "Claire original standing-grab table");
  ```
  Register CTest and observe FAIL with native player-link offset pending.
- [x] Copy all source float literal pairs from pinned en01.c1347..2042 into
  constexpr arrays (660pairs). Implement checked dispatch; add independent
  source verifier comparing all1320float32 words/counts. Run both tests GREEN.

## Task2: complete source Dansa services

Files: src/collision.hpp,new src/collision_steps.cpp,
new tests/collision_step_tests.cpp,CMakeLists.txt.
Produces first-hit merged area index, state flag update, and StepLimitResult
(state_flags,remembered_area,vector<CollisionRecord> walls).

- [x] Add pending interfaces and fixtures for inclusive boundaries, ordering,
  state30gate, inactive/type/prm/floor filters, ignoredY, old-area fallback,
  all4literal wall coordinates, adjacent areas, partial/empty limits,
  zero/no old area, nonfinite consumed data and invalid remembered index.
  ```cpp
  auto result=build_step_limits(room,{},body,{});
  ck(result.walls.size()==4 && result.walls[0].attribute==0x800000,
     "source temporary step boundary order");
  ```
  Build and observe FAIL with native step-area query pending.
- [x] Implement CheckDansaAtari/CheckDansa/SetDansaLimitAtari fully with pure
  float queries and no global wall mutation. Preserve source fallback and
  unconsumed data. Run new and existing player collision checks GREEN.

## Task3: original room/table coverage and final gates

Files: tools/room_inspect.cpp,docs/scenario-status.md,this plan.

- [x] Probe all205rooms' authored step areas and four limit directions using
  actual merged metadata; check native clip401/402/404/405 table ranges against
  original Claire clips on ten initialized graveyard roots. Keep all prior
  NG00/NG01/NG02/grab/corridor checks. No full PlayerLink world alignment claim.
- [x] One fresh read-only final review; material findings one RED/GREEN fix and
  full suite; Minor findings recorded/deferred.
- [x] tools/build.ps1:62checks and updated EXE/ZIP, final205room scan and fresh
  packaged dummy-SDL smoke241ticks/240renders/53moves/10blocked/menu/INI creation.
  Format/diff gates, evidence/rulings ledger and commit. Full Step1 active.

## Evidence

- Pending lookup RED, then both table/lookup checks GREEN:48source samples,
  220Chris/player3reuse frames and all1320source float32 words.
- Pending Dansa query RED, then4focused checks GREEN, including existing
  player collision. North-only remembered fallback with unused NaN depth
  separately RED -> side-specific consumed validation -> GREEN.
- Original probe initially used the base Claire bank and failed range checks;
  source PlyDG01 selects the initialized enemy bank. Corrected probe passes
  all205rooms:57step areas/570static+merged center/edge points/430temporary
  boundaries N/E/S/W86/108/134/102;2200Claire table frames across10roots.
  Previous NG02/grab/corridor probes retained.
- One fresh gpt-6-astra/high read-only review found no Critical/Important/Minor
  issues. Final tools/build.ps1 passes62/62checks and refreshes EXE/ZIP.
  Final205room scan retains the above counts and all previous probes; fresh
  packaged dummy-SDL smoke passes241ticks/240renders/53movementticks/
  10focus-blocked ticks/main-menu return/INI creation. Step1 remains active.
