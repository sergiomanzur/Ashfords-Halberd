# Stateful WallEx/Wall2 and PlayerLink implementation plan

> Execute inline with superpowers:executing-plans and test-driven-development;
> one fresh read-only whole-increment review at the final gate.

**Goal:** Complete the source collision transaction and native PlayerLink world
alignment, including original step walls, push state and wall-damage timer.
**Architecture:** Reuse the existing geometry with internal ordered hit hooks
and merged dynamic walls. Stage small collision metadata and touched work
scalars, player state and immutable effect pool; commit only after all consumed
dependencies validate. PlayerLink stages source Ninja transforms and delegates
to the complete Wall2 transaction. Source Effect29 restores trigger attr40 after
32eligible control ticks and expires; room transition already clears the pool.
**Tech stack:** C++17/CMake/CTest/Python, pinned MIT source, Windows x64.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Base:** cfc2a4f clean. Prior goal turn was progress: complete authored tables/
Dansa services,62checks/205rooms/fresh packaged smoke. Full Step1 active.

## Constraints and review focus

- Headless only, isolated ignored profiles, private retail assets.
- All WallEx types0..5/7/static then dynamic; type6/default no response. Keep
  previous-position rectangle quadrants/face gaps and proposed ledge gaps.
- Dansa uses actual work position and radius; appended type1 limits are removed
  before Wall2's original-proposal bhCheckWallType query. No permanent leftovers.
- Rectangle push uses rounded yaw and source forbidden-direction attr bits,
  inclusive backward5point; corner branch resets count even without projection.
  state80 stamps attr20000 and merged push index before geometric response.
- Special circle attr40/state40000000 and global playerflags6clear sets packed
  player modes2 or102, flags10004 and systemflags bank5 bit4. Birth allocation
  failure leaves attr40 set. Source saved work flags are restored by Wall2 even
  when work aliases the global player; modes/systemflags/effects still persist.
- Effect29 uses first slot !(flags&3), retains scratch and other effects,
  unit scale/zero base scale/texture-1/player position/bit-reinterpreted player
  angles as offsets. Explicit merged wall linkage is distinct from enemy parent.
  Gate (spflags8||effectflags1), active1/notstate1000000, ct=(ct+1)&31;
  expiry restores linked wallattr40 and flags0, even if that wall is inactive.
- PlayerLink transforms EXP0anchor by enemyXYZ/world; writes playerXZ only,
  yaw=uint16(enemyay+waxp), old position from per-work mtx rather than shared
  model cache. flags1000 table lookup uses frame/65536 and preserves its exact
  authored offset; rebuild mtx with yaw_correction, otherwise retain old mtx.
  Wall2 correction goes to enemyXYZ, not player proposed position. No double
  application or eager copies of models/poses. Malformed late query rolls back
  wall attrs, step/push/player state and effects as well as world transforms.

## Task1: complete source stateful collision and timer

Files: new src/collision_response.hpp,src/collision.cpp,src/scenario.hpp,
src/effects.hpp,new src/stateful_wall.hpp,new src/stateful_wall.cpp,
new src/wall_damage_effect.cpp,src/runtime.cpp,CMakeLists.txt,
new tests/stateful_wall_tests.cpp.

Interfaces:
```cpp
struct StatefulWallResult {
  Vec3 position{}, displacement{};
  size_t hits=0;
  std::optional<size_t> wall;
};
StatefulWallResult collide_stateful_walls_ex(ScenarioState&,ScenarioEntity& work,
    ScenarioEntity& player,Vec3 proposed,Vec3 previous,float radius,float height);
StatefulWallResult check_enemy_wall2(ScenarioState&,ScenarioEntity& work,
    ScenarioEntity& player,Vec3 proposed,Vec3 previous,float radius,float height);
void tick_wall_damage_timers(ScenarioState&);
```
Work additions: int32push_count,optional merged push_index/step_area.
State addition: uint32effect_flags. Effect addition: optional merged linked_wall.

- [x] Add pending interfaces and source fixtures; register stateful-wall test;
  watch FAIL with Native stateful WallEx pending.
  ```cpp
  auto hit=check_enemy_wall2(s,p,p,{2,0,2},{-5,0,2},1,10);
  ck(hit.wall==0 && p.flags==0x100,"Wall2 flags/query use original proposal");
  ```
- [x] Add shared ordered geometry hooks for rectangle/circle/corner and dynamic
  records, leaving the existing pure response API behavior intact. Implement
  every stateful side effect and temporary-wall cleanup in one staged service.
  Tests: all four push directions and blocked masks, source corner reset,
  static/dynamic order, stamp/index, full/partial pool, both packed modes,
  global-player suppression, same/different work flag restoration, step actual
  radius/position vs proposed probe, original-vs-corrected type query, hits1,
  flag100snap, type6/default no-op, late malformed rollback.
- [x] Implement Effect29 callback and runtime order after player/before objects.
  Tests: first free/reserved slots, full pool, complete birth defaults, scratch
  retention,31ticks/no restore then32/restore/expire, gated/frozen/inactive,
  inactive wall on expiry, dynamic linkage and late malformed expiry rollback.
  Run new and existing player collision/step/effects/runtime checks GREEN.

## Task2: complete native world alignment and original joined probes

Files: src/zombie.hpp,src/zombie_player_link.cpp,new tests/player_link_world_tests.cpp,
tools/room_inspect.cpp,CMakeLists.txt,docs/scenario-status.md,this plan.

Produces `void link_zombie_player(ScenarioState&,size_t root,ScenarioEntity&);`
Consumes original offset lookup and complete check_enemy_wall2 above.

- [x] Write world/matrix fixtures and pending implementation; observe FAIL
  with Native PlayerLink world alignment pending. Tests include all characters,
  four clips, anchor rotation/retainedY, tableframe fractional truncation,
  yaw_correction distinct from actual yaw, old per-work matrix distinct from
  cache, flags1000clear ignoring unused clip/character, collision enemy-only
  XYZ correction, malformed/alias/late collision rollback.
- [x] Implement source transforms and complete Wall2 call with staged scalars
  and no actor/model copying. Run tests GREEN and full joined original Claire
  NG02/PlyDG01/PlayerLink sample/CalcEnemy on ten roots/both facings/startups.
  Preserve original metadata probes and all previous grab/corridor gates.
- [x] Fresh read-only review once; material findings one RED/GREEN fix pass,
  minor findings recorded/deferred. Full64checks,updatedEXE/ZIP,205room final
  scan and fresh packaged dummy-SDL smoke241/240/53/10/menu/INIcreation.
  Format/diff/status evidence and commit. Full playable slice remains active:
  complete encounter dispatch, armed controls/combat, inventory/audio/saves.

## Evidence

Pending WallEx RED -> complete source response/timer GREEN. Pending PlayerLink
RED -> complete transforms/transaction GREEN. Four focused tests (new stateful,
world alignment and existing player/step collision) pass, including literal
dynamic shapes0..5/7, all triangle IDs and odd-type filtering.
The first world fixture assumed an X correction; source prior face gaps4>3
select Z=5, deltaZ+3. Its expectation was corrected; production geometry stayed.
Original205room probe passes4400joined NG02/PlyDG01/PlayerLink world/matrix
updates and666source wall corrections across10roots/both facings/startups,
with all temporary walls removed. Prior4680enemy/4680Claire/9280paired samples,
40deaths/releases/320bloodbirths,57step areas and corridor checks remain passing.

Final verification: full headless build exits0,64/64 checks pass and EXE/ZIP
are refreshed. Final original-room scan exits0 with205rooms,4400PlayerLink
updates/666wall corrections; report has206lines including header. Fresh
packaged dummy-SDL smoke exits0:241ticks/240renders/53moves/10focus-blocked
ticks, main-menu return and INI creation. Diff/format checks pass. All build,
scan and smoke processes are terminal before commit.

One fresh source reviewer found no Critical/Important defects. Deferred
minors: reset effect_flags on room initialization when adding the effect-control
override (no production setter currently enables it); add focused late-circle
birth rollback and outer enemy-correction overflow fixtures. Existing late
query/timer rollback fixtures and source inspection cover transaction staging.
This increment completes these providers; it does not complete Step1.
