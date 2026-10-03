# Native scenario execution status

Step 1 remains incomplete. The room event decoder and initial control subset
are native C++; the application has not yet integrated scenario execution.

`scenario.cpp` decodes the event block referenced by outer room header +28.
EVT_WORK's offsets are relative to that block. Slots 0 and 1 are initialization
and per-frame control, while event IDs begin at slot 2, as in `Event_init`.
The first offset bounds the offset table. Programs are bounded by the nearest
following outer-header section, allowing distinct entry points to share tails.
All entries share one immutable event block. All 205 retail rooms passed native
decoding, totaling 4,363 entry slots.

The synchronous interpreter supports END, IF/ELSE/ENDIF, CK, SET, CmpW,
PlItemCheck, ArmsItemCheck, ItmCk, ItmSetCk, InitModelSet, EtcAtariSet2,
ItmAspdSet, SetDoorCall, RoomCaseNo and RoomSoundCase.
CK/SET preserve MSB-first flags, bank capacities and SET's six
single-bit/tail operations. CK also checks interaction/floor indices for the
callback bank. IF handling preserves the source G_Sp target cursor rather than
substituting a conventional stack. Truncated instructions, invalid branches,
invalid flag accesses and unsupported opcodes fail explicitly. An instruction
budget prevents a malformed loop from hanging the process.

The native scheduler retains 16 task slots, program pointers, four counters and
the shared conditional cursor. EvtOn supports explicit slots and automatic
allocation, including the original slot-15 overwrite when all slots are active.
EvtKill, Sleep/Sleeping, EvtNext and EvtEnd preserve pointer advancement and
yield behavior. Scheduling is gated by special flag `0x10`; the event timer
increments once per enabled pass with at least one active task. A newly started
higher slot executes in the same pass, while a lower slot waits for the next
pass. Replacing the current slot retains its running instruction pointer as the
original scheduler does. Sleep or yield inside a conditional continues through
the shared branch cursor before returning. FOR/NEXT preserve signed 16-bit
initial counts and cached count2/count3. WHILE/EWHILE retain six body/condition
pointers, and EWHILE tests the condition at the loop tail. Supported loop
conditions are CK, CmpW, PlItemCheck and ArmsItemCheck; others fail explicitly.
Loop types share the nesting index, with four counter slots and six pointer
slots. Event initialization retains existing counters and loop pointers, as in
the reference. Wider entity controllers are still pending.

Inventory conditions inspect the original player block's slots 1–11 in the
384-word inventory and retain item 72's high-byte room-flag side effect. CmpW
supports the original player/enemy health, message selection and status timer
fields, its six comparison operators and unsigned 16-bit right-hand values.
These are native state conditions; inventory management and UI are pending.

Item commands update collected/availability flags, item visibility and pickup
trigger activation. They preserve the different source masks for item lookup
and IC clearing. Model visibility commands resolve authored enemy indices
through the runtime allocation map and preserve dead-enemy disabled flags.
The stage-9/room-30/case-2 player-hide sleep request is retained. Interaction
rewrites preserve geometry and flags, and decode all four parameters and type.
The caller still binds runtime entity state; these commands do not instantiate
models, simulate enemies or implement inventory UI.

`AshfordRoomInspect RDX_LNK.AFS report.tsv --scenario-check` executes the
complete retail RM_0010 initialization program without skipped commands. Four
states cover map flag 34 clear/set and items 123/124 uncollected/collected.
They verify both exit-trigger definitions and pickup persistence. The first
path creates a direct door; the return path creates an examination trigger.
Sixteen full idle control passes cover two ticks and two equipped-weapon values
for those four persistence states. They verify the event starts once and item
rotation speeds follow the equipped weapon and collected state. Eight scripted
examination callbacks cover item flag 4 and event flag 120, verifying the room
case, room sound case and destination request (stage 0, room 2, position 0,
door type 1). These runs execute the full control program but do not tick its
scheduled entity controller, which remains unsupported.

SetDoorCall emits a decoded transition request. Applying `bhSetDoorDemo`'s
player/input/fade state changes, selecting the archive room by stage/room/case,
loading the destination, and preserving persistent progress are still pending.
The current subset can execute corridor initialization and tested control paths.
Other interaction branches, the scheduled common entity controller and the
graveyard scripts still need additional commands. RoomSoundCase preserves the
reference setter's state; native audio playback remains to implement.
Unsupported commands are not skipped.

Scheduled LoadWork now binds authored enemies through their allocated-work-slot
map, or binds objects/items directly, retaining the original model index and
enemy motion-state cache rules. CommonCtr sign selectors and whole-actor
position/rotation assignments (subcommands 7, 11, 12 and 13) are native.
Positions use unsigned hundredths with independent sign bytes, update backup
coordinates and select the floor from the bound room ground table. Rotations
retain signed integer angles from the original degree conversion and update
backup angles. Bindings use stable indices and are checked again on access.
Synthetic scheduled tests cover those transforms, allocation indirection,
cached state, invalid bindings and missing floor data. CommonCtr subcommand 47
now rebuilds the bound primary actor model from its current joint pose and
whole-actor transform, producing a native skinned, textured scene. The node
index from LoadWork does not select another model. Linked actors and retained
external matrices fail explicitly until their matrix ownership is implemented.
CommonCtr motion selection supports mode 1 with calculation option 1, binding
the room motion bank, preserving the high motion-number bytes, and setting
source speed/mode/blend fields without immediately posing. Other selection
modes, calculation options and companion updates remain unsupported. FrameCheck
supports player, mapped enemy and object 16.16 frames, including the half-frame
offset. The native actor update now implements the corridor's `bhSubpl` path
(ID 67, initialization and mode 1 controllers 0/10), including startup,
motion-number replacement,
16-step pose blending, attributes sampled before frame advancement and model
rebuilding. It runs before the event update, as in the original game loop.
The headless probe executes the unchanged corridor event through frame 149
and exit using this actor update, with no assigned completion frame, then
verifies the 150-frame loop wraps. Regular motion banks are loaded in runtime
slot order with the original per-ID reuse. The probe initializes the corridor
actor and verifies its empty-bank hold before script selection. Other actor
controllers and their initialization,
mirroring, linked matrices and joint overrides remain to port. The application
preview loop is now connected to these native updates; full gameplay remains pending.
`bind_room_enemies` now constructs the original 128 runtime slots from active
authored records, retains complete immutable model banks, aliases the primary
model, calculates floor and assigns the regular motion banks in runtime order.
The script lookup map preserves ETTY `work_number` values, including inactive
entries, rather than inventing a map from allocation order. Binding replaces
enemy state only after validation, preserving inventory and persistent flags.
Synthetic tests cover compaction, signed yaw, ignored authored X/Z rotation,
ID-dependent model flags and failure rollback. The retail probe binds enemies
in all 205 rooms and uses the resulting corridor state for startup and its
complete event. Object/item asset loading and bindings now exist; their
remaining special controllers and complete gameplay remain pending.
Player/effect bindings and sound commands remain unsupported.

Native `initialize_scenario` follows `bhInitEvent`'s state/script ordering:
deactivate old tasks without erasing their counters, run enemy control, reset
conditional depth and event timer, execute room initialization with task 0's
command context, then give scheduled events one pass under the temporary
0x10 scheduler flag and clear that flag. Initialization starts at script PC0
without storing its final PC over an event that it starts in task 0.
`control_scenario` executes room control in the retained command context and
then schedules events. Every enabled scheduler iteration assigns that context,
including inactive slots; a disabled scheduler leaves it unchanged. Tests
cover initialization command binding, a yielding task-0 event, counter
retention, context 15 after scheduling and room-control cursor isolation.
The headless retail probe runs all four corridor initialization states and a
complete initialization/control/actor/event sequence through the 150-frame
animation. Sound commands 3/4, remaining special object controllers and
complete gameplay still remain to implement.

Native `bind_room_props` carries decoded room objects and pickups into the
script-addressable vectors. Object slots 0–3 retain player attachments; room
slots and pickups are rebuilt with authored metadata and immutable model
ownership. Tests check signed transforms, type/parameter splitting, source
flags, retained attachments and rollback on malformed assets. The retail
corridor initialization probe now uses these bindings rather than placeholder
pickup entries.

`load_room_runtime` now assembles scene geometry, cameras, collision, authored
player entry, player pose and scenario resources into one replacement runtime.
It applies the represented system room-change masks, retains persistent
inventory/event/map/item flags and attachment slots, then enables the original
subsystem gates before initialization. The healthy unarmed player samples pose
0 and advances its frame counter before scripts run, matching normal room
entry. A synthetic initialization script checks frame 1; tests also verify
flag masks, retained task counters and failure isolation. Armed entry, save
restoration and the introduction require separate controller paths.

`AshfordRoomInspect RDX_LNK.AFS report.tsv --runtime-check SYSTEM.AFS` loads
Claire's original package and uses this shared runtime loader for all four
corridor initialization cases. The existing scenario diagnostic remains
available without a player archive. The complete New Game entry remains
pending; this loader alone does not complete the playable slice.

The shared `tick_room_runtime` now updates represented actor controllers,
healthy unarmed player movement/collision/posing, simple props and pickups,
then room control/events. Player frame checks use the updated motion counter;
the player and prop subsystem gates are respected. The retail runtime probe
runs 240 ticks for each of the four corridor initialization cases, including
60 ticks of forward input, and checks player/scenario frame synchronization
and pickup model rebuilding. This shared simulation loop is now wired to the
application's corridor preview.
The retail probe passed 960 joined ticks, with player displacement on all
240 ticks of forward input across the four entry cases.

`tick_scenario_props` implements original dummy object dispatch, type-2 yaw
rotation, type-3 oscillation and pickup highlight timing. Pickup highlights
cycle through the original random delay and eight 8192-angle steps, rather
than rotating the item's base model. The 32-bit LCG and 31-bit return were
verified against the USA ELF's `rand` routine at 0x125320. Its seed lives in
scenario state and survives room entry. Tests cover the complete highlight
cycle, seed values, object rotation, previous transforms, subsystem gates,
hidden-object sleep behavior and unsupported matrices. Linked/retained
matrices, weapon objects, object collision, alpha fading and special object
controllers fail explicitly. The highlight drawing overlay, effects, light,
sound, interactions, armed/damaged player states and complete gameplay
remain pending.

`compose_room_runtime` now combines prepared room, player, enemy, object and
pickup geometry for the existing depth-tested camera renderer. It preserves
room visibility indices and actor indices, remaps each texture pack without
altering the untextured sentinel or cached poses, and checks resource bounds.
The represented drawing flags, hidden states, object category gates and cut
visibility rules control inclusion. Linked/retained matrices and morphing
draw modes still fail explicitly. Tests cover texture remapping, sentinel and
capacity boundaries, immutable inputs, hidden pickups and object categories.
The retail runtime probe rendered 16 combined original-camera snapshots
headlessly across its four entry cases. Lighting and highlight overlays remain
pending; the application corridor preview now uses this input/render loop.
Each of those 16 images differed from the same camera's room-only render,
confirming that runtime entities contributed visible pixels.

The application now exposes a main-menu **Corridor preview** using the shared
native runtime. WASD/arrows and controller stick/D-pad control baseline
movement; Shift/controller X runs; Escape/controller B returns to the menu.
Simulation runs at a fixed 60 Hz with bounded catch-up, and prepared geometry
is drawn through SDL using original camera cuts. Display preferences continue
to use the existing INI. Focus loss suppresses keyboard/controller movement.
The preview is not the completed Step 1: interactions, connected-room travel,
enemy combat, inventory, audio and save/load still need implementation.

`tests/native_runtime_smoke.py EXE RDX_LNK.AFS SYSTEM.AFS fresh-profile` performs
headless application verification with SDL dummy/software drivers. It uses
the user's retail assets, injects keyboard input and a virtual controller,
checks focus loss with a held stick, renders 240 frames and returns to the
main menu. It is a separate optional retail diagnostic; no game assets are
added to the standard synthetic CTest fixtures or distribution.

The retail RM_0010 control script sets RoomCaseNo before SetDoorCall into
stage 0, room 2, position 0. Persistent flags choose the first-visit or return
case. RM_0020/RM_0021 contain the return door into room 1, position 1. These
records establish the candidate connected pair without inventing room links.

Native examination now selects authored interaction records using the original
five forward probes, floor checks, facing masks and first-match ordering.
Door requests retain the authored destination; script-only examination feeds
the selected index into scenario control; standing pickups request the original
pickup interface without prematurely awarding inventory. The runtime triggers
examination on a new button press, so holding the button does not repeat it.
Tests cover probe offsets, cardinal facing masks, inactive records, floor
matching, blocked doors, pickup binding and press/release behavior.
Stairs, ledges, kneeling pickups, item boxes and examination camera/message
interfaces still require their original controllers and fail explicitly.
Physical examine input, confirmation UI, inventory awards and room transition
consumption remain pending in the application preview.

Scripted door calls now reproduce the represented `bhSetDoorDemo` state changes:
freeze normal gameplay, retain the event timer gate, set player/door control
state, and leave examination mode using separate drawing flags. Successful
`transition_room_runtime` loading consumes the pending door only after the
destination validates, restores subsystem gates, and preserves inventory,
progress and the random seed. Invalid destination data leaves the old runtime
and door request intact. Synthetic checks cover destination position, atomic
failure, persistent state and actual scheduler advancement after loading.
`room_archive_index` resolves all 205 pinned USA room variants, including cases,
and rejects missing identities without substituting another variant.

Retail corridor examination/control checks continue to pass. The optional
`AshfordRoomInspect RDX_LNK.AFS report.tsv --transition-check SYSTEM.AFS`
currently fails explicitly at graveyard initialization: room 2, actor ID 1,
mode 0. `en01.c::bhEne01_Init` requires the zombie controller and linked enemy
work for body parts before the destination can run. Porting that controller
is the next room-travel dependency; application travel remains unavailable.
Door presentation, audio and a complete two-room playable slice remain pending.

`allocate_enemy_work` now ports `eneset.c::bhSetEnemy` for the original fixed
128-slot pool. It chooses the first inactive work slot, resets retained state,
sets authored type/model version/position/yaw, recalculates floor, and follows
the original model flags and final clipping-entry overwrite order. Room enemy
binding uses this allocator while retaining the existing authored mapping and
motion-loading order. Exhaustion returns no slot; invalid context or nonfinite
positions/ground fail before changing the pool.

`link_zombie_enemy` ports `en01.c::bhEne01_SetLinkEnemy`: allocate the original
ID-1 `zob_lkmtab` before replacing its ID, set hidden linked flags, share the
parent's immutable model/motion resources, and store parent/joint references
as native indices. Tests cover recycled work, clipping lists, exhaustion,
malformed context and ownership after state copying. The retail diagnostic
checks six core linked-work bindings per zombie in both graveyard variants.
These are allocation/ownership checks. Linked placement and channel application
have the probes described below; zombie startup, AI, attacks/damage, collision
and body drawing still require their controllers.
The actual transition diagnostic continues to reject actor ID 1 at mode 0.
The selected initial graveyard contains five type-0 zombies (models 9 and 32)
and actor ID 91; both zombie and subplayer controller paths remain necessary.

`zombie.cpp` now ports the original personality/model lookup, normal/easy health
tables, signed integer health adjustment, and root walking-pair selection.
The lookup preserves the source's 30-row iteration limit, so model 43 retains
the original fallback rather than selecting the table's final row. Walking
selection skips linked/inactive/non-zombie work, cycles from the preceding
initialized root, and consumes random state only for the first root. The USA
ELF RNG is shared with pickup timing, preserving wrapped 32-bit state and its
31-bit return value. Missing preceding extended work fails explicitly.

All 16 `bhEne01_InitType` table entries are adapted: root versus linked-body
motion selection, half-health and parent-health restoration, extended flags,
Type10 collision flag clearing, and Type15 zero motion speed. Tests exercise
every entry for both roots and bodies, retained frame state and RNG cadence.
The retail diagnostic verifies ten graveyard zombie parameter/type selections
and their actual startup/walking motion resources across both variants.
These helpers cover the type subroutines, not complete `bhEne01_Init` or an
enemy encounter. Surrounding model/linked-part setup, shadows, pose/collision,
movement, attacks/damage and full controller dispatch remain required before
the application can enter the graveyard. Actual room travel still rejects the
unimplemented zombie controller explicitly.

The linked transform prerequisite is now available through the actor renderer:
world joint queries and baseline parent-joint/local-offset/XYZ placement share
one skeleton traversal. Retail coverage includes 30 graveyard skeleton samples
and 180 whole-model linked poses. Body subtree selection,
rotation-reset flags and the complete enemy controller remain required; the
diagnostic poses do not enable application travel or an enemy encounter.

`apply_motion_pose` now supplies bounded full/body channel mapping and the
original mirroring, work flags and six motion-application branches. Shared
blend-angle scratch is owned by native scenario state and used by both actors
and the player, preserving cross-call behavior without process globals.
The corridor/player missing-channel regressions failed before integration and
passed after it. Retail checks cover 480 graveyard startup/walking applications
with mirrored/blended samples and the body's joint-eight offset. Complete
zombie initialization, body subtree drawing, movement/attacks/damage and ID-91
subplayer handling remain required; application graveyard travel stays pending.

`initialize_zombie_work` now adapts the selected ordinary main startup path
for model versions 9, 10 and 32. The second graveyard variant contains version
10 roots in addition to 9 and 32. Native extended work retains the body and
first seven child slots across reinitialization, resets other startup state,
allocates six core works per root and the extra first-root hidden manager,
and preserves the original health/walking RNG order. Attached body work has
its own counters while applying joint-eight channels to the parent's mutable
pose through an owner index. Copying a scenario isolates those poses.

Initial collision dimensions, damage clearing, flags, clipping joints and
type dispatch follow the source. Root startup reads the finish-room bind
matrices; attached-body startup rebuilds the combined parent pose and obtains
capsule endpoints and height from world joints. Startup shadow descriptors and
the default material color are retained as metadata; their rendering is still
pending. Stair-use clearing has no native stair binding yet. Detached suffix
calculation and saved-matrix/rotation-reset model work remain unsupported.

`initialize_zombie_auxiliary` adapts head, arm, leg and manager startup from
`en01sub.c`, including original extended-work sizes and selective resets on
reuse. Its damage controllers and the manager's nearest-root/stair movement
checks are still pending. These helpers are exercised by the developer probe;
they are not yet dispatched by the runtime enemy scheduler. Transactional
main startup/motion helpers replace `ScenarioState` on success, so callers
must reacquire entity references. The motion helper is currently diagnostic;
per-frame full-state copying needs replacement before gameplay integration.

Synthetic tests cover allocation/reset order, RNG, shared pose ownership,
copy isolation, variant-10 health, auxiliary reuse and failure rollback. The
retail runtime probe verifies 20 root/body initializations, 62 allocations and
52 auxiliary initializations across both graveyard variants, while retaining
the 480 motion-channel probes. This advances the native enemy startup work.
AI, combat, ID-91 control and the rest of the Step 1 playable slice remain
incomplete, and New Game remains unavailable.
