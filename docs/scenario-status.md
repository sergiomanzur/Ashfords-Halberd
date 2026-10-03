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

Inventory conditions inspect the original player block's slots 1â€“11 in the
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
script-addressable vectors. Object slots 0â€“3 retain player attachments; room
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
controllers remain unsupported; the general saved-matrix/range calculation
dependency is now implemented as described below.

`initialize_zombie_auxiliary` adapts head, arm, leg and manager startup from
`en01sub.c`, including original extended-work sizes and selective resets on
reuse. Original mode-0 startup, mode-1 empty head/arm/leg callbacks and the
manager's nearest-root/stair policy now dispatch through the runtime scheduler
as described below. Detached-part damage controllers remain pending. Transactional
main startup/motion helpers replace `ScenarioState` on success, so callers
must reacquire entity references. The motion helper is currently diagnostic;
per-frame full-state copying needs replacement before gameplay integration.

Synthetic tests cover allocation/reset order, RNG, shared pose ownership,
copy isolation, variant-10 health, auxiliary reuse and failure rollback. The
retail runtime probe verifies 20 root/body initializations, 62 allocations and
52 auxiliary initializations across both graveyard variants, while retaining
the 480 motion-channel probes. This advances the native enemy startup work.
Root AI, combat and the rest of the Step 1 playable slice remain
incomplete, and New Game remains unavailable.

Zombie skeleton/capsule calculation now retains native `ActorModelWork`
instead of a separate transient matrix vector. Saved joint matrices affect
descendants while preserving siblings; the attached-body renderer consumes
the same cache as collision. A copied-state regression freezes joint eight,
reinitializes its body, and verifies parent capsule height and isolation from
the original runtime. General selected-range calculation/drawing and linked
rotation reset are now available in the actor renderer. Detached body logic
and rebased skin lists remain pending; the runtime scheduler still rejects
the unsupported graveyard controller rather than exposing a static encounter.

Native auxiliary dispatch now retains the original shared callback registrations
made by `bhEne01_Init`. IDs 31â€“33 and 38 can be reused by other enemy types;
unregistered or unsupported bindings fail instead of being silently assigned
zombie behavior. Registered callbacks persist with the native scenario state.
The indexed enemy loop preserves original pool order and suppresses linked
callbacks when the parent has hidden status flag 0x1000000.

The parent manager selects the first eligible root at the minimum cached
EXP0 distance84, clearing and setting only the source's nearest bit in extended
secondary flags. Eligibility preserves active ID1, initialized extended work,
type not10, unlinked/visible/enabled flags. The stair policy preserves the
original stage0 **or** room6 condition: two or more eligible roots on floor2
restrict eligible roots on other floors, while below that limit their restriction
clears. Unmatched stage/room keeps prior stair bits. The original ten-slot
scratch capacity is checked; malformed distance or overflow cannot partially
commit manager flags. Per-frame manager work stages scalar fields without
copying render geometry or entire scenario state.

Successful represented callbacks now run `zonzon1.c::bhEne_InitDamage` afterward.
It clears only the current ML_WORK object count's damage entries, decrements
nonzero combo timeout with native word wrap, clears combo bits 0x70 and flg2
bit4, and preserves status flags. Model ownership and prepared facial counts
are respected. Null model work retains damage entries. The cleanup applies to
startup, held and moving subplayers and auxiliary callbacks; suppressed or
unsupported callbacks do not perform it.

Tests cover nearest ties and exclusions, cached distance, the original stair
OR condition, floor counts, scratch overflow, registered/unregistered callbacks,
hidden-parent suppression, original empty idle behavior, cleanup ownership and
joint bounds, negative timeout wrap and prepared facial object count. The
headless retail probe dispatches 3,328 native auxiliary frames, including 126
manager updates, across both graveyard variants with actual initialized model
owners and unchanged RNG/raw assets. It isolates these callbacks from the
pending root AI; it does not claim the full encounter runs. The root zombie
movement, attacks/damage and player response remain required before travel
into the graveyard is exposed. Step 1 and New Game remain incomplete.

Native enemy targeting prerequisites are now implemented in
`enemy_targeting.cpp`: `bhSearchPlayer`/`bhSearchPlayer2` preserve the source's
two Ninja table rotations, 3D normalization, strict view-cone comparison,
zero-cone fallback, signed atan yaw and valid-1 sentinel remapping. The explicit
probe API preserves actor work without temporary position changes.
`NitenDir_ck` retains its XZ-only query. `bhEne01_SideRotNeck` preserves the
910-word step, snap comparison, 16-bit wrapping and source neck limits.
`bhEne01_RotNeck` resolves shared pose ownership, reads cached world matrices,
uses the parent yaw for linked bodies and updates only the selected neck yaw.

Source review exposed a signed initial-angle mismatch in the existing pose
representation. Joint poses now retain signed32 CNK object angles from the bind
model; motion samples and interpolation still produce original unsigned16
angle words. The regression first failed with a raw neck yaw-2000 and forward
target (wrong64446), then passed with the original62626 result; the next update
uses that canonical output word and produces63536. Renderer inversion masks
to the angle word before negation, avoiding overflow on raw signed extremes.
Facial tooth/tongue and inverted eye copies also retain their full source
integers; the eye copy regression fails on truncated65432 and passes on-104.
Prepared jaw assertions now check signed rotations while retaining the separate
clone-rotation invariant.

`bhEne_EnemyAtariCheck` now queries the first qualifying authored/dynamic floor
trigger with active/type2/id/action/floor matching, inclusive XZ boundaries,
ignored Y and static-before-dynamic order. Returned records retain their
original owner; this query does not invent behavior from other floor attributes.
Tests cover malformed consumed values, cache/owner/joint errors, negative signed
authored neck angles and rejection without partial pose changes.

The private retail targeting diagnostic queries318 authored floor trigger
centers and ten actual graveyard personality view cones, and exercises640
isolated root/body neck updates against cached Claire joint5. These are
controller prerequisites; the full root zombie frame sequence, Brain routing
integration, movement, damage, attacks and player response still require their
original state handling. New Game and graveyard travel remain disabled until
the real playable slice is ready.

Native `bhEne01_EneSearch` now updates the original packed awareness byte and
timeout through cached neck joint11, source probeY0, own actor yaw plus signed
shared neck yaw, and the retained EXP0 personality index. It promotes pending
view to the latched bit, preserves the31-frame counter cycle, clears the view
latch at the last failed check and stages only changed scalar fields. Invalid
consumed context cannot partially update awareness, timer or flags. Startup
and reinitialization reset the new byte fields with the original extended work.

The unsigned timeout behavior is confirmed against the private USA executable:
0x17a504 loads an unsigned byte,0x17a508 decrements,0x17a50c stores,0x17a510
masks0xff and0x17a514 branches on a nonnegative value. The source's negative
timeout flag-clear branch cannot run. Native awareness preserves this behavior;
a260-frame loss-of-view regression verifies timer wrap with notice bit0x400
retained. No signed-timer gameplay change is introduced.

`decode_room_routes` now reads ROM_WORK route regions at0x28, next-hop bytes
at0x2c and count at0xa8. It preserves bounded36-byte ATR records, finite XYZ
values and the full count-squared matrix, with at most255 byte-indexed regions.
Zero-route rooms ignore unused pointers. Native room loading binds an immutable
route snapshot and replaces it only with successful room load; failed
transitions retain the prior routes and pending request.

`bhCheckRouteID` uses inclusive XYZ bounds, ignores ATR flags/IDs/floor, and
selects the first region. `bhCheckRoute` indexes destination rows/source columns,
copies the destination and returns0xff for a shared region, and preserves
output when either point has no region. Other valid queries return a next-hop
region center. Unused diagonal0xff values survive decoding; a selected
out-of-range hop fails explicitly before writing output.

The retail scan decodes2,688 route regions, exercises85,944 valid center-to-center
queries and excludes19,556 center-pair probes selecting source hops outside
the declared region array. It also runs1,860 isolated awareness updates across
ten initialized graveyard root/body pairs. These checks retain205-room decoding,
auxiliary/manager dispatch, targeting, contacts and corridor runtime evidence.
They do not imply full zombie Brain/Move/attack/damage dispatch is ready;
unsupported root controllers still fail explicitly and New Game remains
disabled pending the complete playable slice.

Native Brain00 now evaluates awareness and stages close-player targets or
original route refreshes from retained distance, obstruction, floor and byte
timer fields. It preserves the exact50 threshold, cached region gate,254->255
refresh,255->0 wrap and type5 unconditional routing. A shared region returns
255 with a defined destination; missing regions reject the source path whose
local target would be uninitialized. Invalid route/stair context cannot commit
awareness, targets, flags, modes or reservation attributes.

The original KaidanCheck now observes player primary/status gates and eligible
competing zombies, then the first authored/dynamic stair query, capacity and
floor direction. Query behavior retains Ninja look-behind, inclusive XZ bounds,
half-unit alignment, ignored Y, and source probe-variable overwrites after a
failed alignment. Pair reservations use stable current-room indices; adjacent
pairs stay in their original static/dynamic array and explicit pairs always
reference the static array. Both records validate before changing attributes.
Primary200000 releases the selected pair and clears notice; ordinary entry
selects modes0/8/0 and flag400000. Source actor indices remain retained after
release. Startup now releases retained stair reservations through the existing
transactional initializer and resets the new extended-work fields. Native room
loading clears prior dynamic miscellaneous records, matching room-system reset.

These are represented controller functions, not full root AI scheduler frames.
Movement, action selection, damage/attacks/player response, inventory/audio and
cross-room save/load remain pending. New Game remains disabled until the full
Step1 playable slice is ready.

The headless retail scan exercises146 authored stair centers and930 isolated
Brain00 close-pursuit updates across ten initialized graveyard roots using
actual cached work and current-room route regions. These traces hold the
player primary4 gate to isolate close pursuit; paired reservations and floor
direction are covered by synthetic tests. The205-room/85,944-route/1,860-
awareness and existing native corridor checks remain passing.

Native MV00 now selects original root2/body202 idle motions, takes its random
frame from the incoming motion count, preserves startup fallthrough and source
RNG order, and selects walking after notice/timer expiration except type8.
Its body-neck oscillation retains unsigned word clamps9102/56434 and sound
countdown runs even outside handled switch states. Motion changes set step
65536 unconditionally; unchanged selections keep other fields, while changed
selections reset frame/blending/mode and source flag bits only. Controller
updates stage relevant scalar work and RNG without copying scene geometry.

Zombie SePlay selection now retains personality offsets, silent/status gates,
active-voice short circuit supplied by its caller, and original voice RNG.
MV00 emits resolved native slot/position/code requests; its low-byte23 call
does not query an active voice. These pending requests are not decoded audio
playback. The audio backend remains required, and room loading clears obsolete
requests together with dynamic room data.

The original ct0–ct3 counters now have distinct native fields. Prop oscillation
uses ct0; pickup highlighting uses ct0/ct1/ct2 while leaving ct3 untouched.
This corrects the prior shifted prop mapping and preserves existing script
and subplayer ct1/ct2 meanings.

SearchNeck now handles linked-body override1000, short torso wax, retained
signed neck yaw, parent/body flag800 and awareness40 unions, strict distance35
and parent-yaw tracking through cached neck/Claire joints. It decays neck yaw
by910 when tracking is inactive, preserves root/mode>=5 no-op gates, and stages
pose/scalar changes so invalid consumed context cannot commit a temporary
neck seed. Prior cached matrices remain unchanged until CalcEnemy.

These native controller functions remain separate from full root MainLoop.
SetMtn fix offsets/effects, movement/action selection, damage/attacks/player
response, decoded audio, inventory and cross-room saves still require their
source implementations before New Game and graveyard travel are enabled.

The private retail scan passed1,350 isolated MV00 calls through ten original
idle-to-walk transitions, with ten retained sound requests and640 SearchNeck
updates against cached Claire joint5. It sampled the selected actual idle
root/body channels once per root; full SetMtn effects and fix offsets were not
substituted with that sampler. All205-room and previous route/pursuit/awareness
checks remain passing.

Native MV01 walking now selects original paired motions0/200,40/240,41/241 or
type5's125/325, retains startup RNG order/fallthrough and short way turning,
and applies the source attack/distance, avoidance and authored stop-window
transitions. Forward/side wall queries use original ar/ah, Ninja trig, static
then dynamic walls, types0–7 and exact source boundaries. They preserve ignored
floor/attribute data, odd-type primary400 gating and raw type7 heights. Current
room loading clears dynamic wall work. Sound requests retain both attack and
cadence calls in a single tick, with scalar/body/RNG updates staged before commit.

Original ikou/ikou3 preserve linked80 no-ops, signed word yaw arithmetic and
negative target reversal. GetTranslateMtn and GetTranslateMtn2 now consume the
selected motion's root position keys, retain the inactive frame/step gate,
mode0 vertical removal, Ninja float ZYX transformation and truncating Y
quantization. The second helper sets scalar spd without moving the actor.
These do not advance motion or replace SetMtn's root-joint zeroing, foot fix,
effects or motion tables. Full Move/action/attack/damage and the playable
Step1 encounter remain pending; root dispatch still rejects unsupported states.

The headless retail scan passes1,200 isolated MV01 policy ticks at retained
frame0/distance100 across ten initialized roots, using actual room walls and
original selected motion pairs. It probes1,272 actual root/body walking keys
through both displacement and speed helpers. This checks decoded channel
compatibility and controller state, without claiming full encounter movement.
All205-room decoding and prior corridor/route/awareness/idle/neck checks pass.

Native bhCalcFixOffset now rebuilds the current signed object path with Ninja
float transforms and subtracts the previous terminal cached joint, including
linked parent/local offsets and source primary1000 behavior. Cached model work
and root-key translation share that arithmetic; renderer-only double transforms
remain separate. Identical current/cached poses produce exactly zero drift,
including the original nonzero ankle offset.

CheckMtnTbl now retains the original fix, sound and vibration tables, verified
against all1,008 integers in pinned MIT source. The callback preserves room-bank
pointer identity, primary40000, motion2/80000, mirror mapping, motion125 scaling,
inclusive windows, leg/parent fix routing and captured pre-sampling event frames.
Corrections, RNG and request queues are staged before commitment. SePlay feedback
is per enemy slot: any active sound suppresses a positive queried code before
consuming RNG; nonpositive queries require matching codes. Room loading clears
sound and vibration requests. These queues still require audio/device output.

The headless retail scan now passes1,610 isolated motion-table calls against
ten initialized roots, retaining28 sound and10 vibration requests. It checks
4,800 exact zero-drift foot paths after fresh caches and480 linear work samples
against an independent child/sibling traversal using source float math. These
checks combine existing diagnostic pose sampling with the table callback;
they do not claim the full source Move/SetMtn/effects/CalcEnemy scheduler.
SetMtn effects and complete enemy action/combat integration remain pending.
The complete Windows build,39 headless tests and package creation pass. A fresh
packaged dummy-SDL profile passes241 runtime ticks,240 rendered frames,
53 movement ticks,10 focus-blocked ticks and return to the main menu.

Native bhEne01_SetMtn now orchestrates shared source channel/counter sampling,
root/body joint0/8 zeroing, root-key translation or scalar-speed movement,
original turning gates, body damage recoil and boundary flags. It retains the
original71/70/40 mouth-shape tables and4x8 recoil table, verified against every
public source constant. Model shape flag/scalar updates preserve the original
inactive/type10 cases. Shared zombie sampling now forwards actual cached O_WORK
flags, preserving protected angles, skipped channels and body offset behavior.

The callback requires synchronous KamiEffect/WaterEffect handlers for mode0<5,
called before mouth/table processing using the incoming pre-sampling frame.
These dependencies are explicit; the source effect backend is still pending.
The frame transaction snapshots only the owner pose and consumed work/parent
scalars, RNG/scratch and appended sound/vibration request lengths. It does not
copy immutable models or rendered scenes. Effect backend storage owns its own
rollback and handlers must keep the fixed work pool/pose ownership intact.

The retail scan now runs1,600 paired SetMtn calls across ten original graveyard
roots, with3,200 explicit dry-effect checks and the existing4,800 zero-drift
foot paths. Diagnostic handlers admit only walking motions absent from the
source Kami table and reject WaterEffect's wet state. They do not implement
the effects backend or register full root Move/action/combat scheduling.
Source review found no remaining Critical/Important/Minor issues after the
O_WORK forwarding fix, whose regression was observed RED then GREEN.
The full Windows build,40 headless tests and package creation pass. A fresh
packaged dummy-SDL smoke passes241 runtime ticks,240 rendered frames,
53 movement ticks,10 focus-blocked ticks, main-menu return and INI creation.

Native bhEne01_CalcEnemy now preserves source cache timing: ordinary roots
consume their prior joint10 height, linked bodies refresh the parent and its
capsule, and detached bodies refresh the parent followed by their own joint8
suffix. Shared O_WORK caches retain the lower prefix and restore the full
range. Separate per-work base matrices preserve primary1000 reuse after a
detached body overwrites the shared upper cache. Frozen joints, negative
heights and radius recovery retain source behavior; updates commit only after
all consumed context validates. Startup uses this callback before its explicit
drawing step.

The capsule's exact minus1 limb gate now reads source EXP88/8c. The previous
misnamed fields represented EXP80/84; these remain distinct and retain their
original motion-table uses. A source review caught the detached endpoint's
parent-cache read when model owners differ; its regression failed before the
correction and passed afterward. No other source-review findings remain.

The retail diagnostic now sequences paired SetMtn and CalcEnemy in work order,
with1,600 cache/collision updates across ten roots,4,800 exact zero-drift foot
paths and30 headless posed draws including20 changed frames. CalcEnemy does
not morph or draw geometry; source mouth morphology belongs to later
bhDrawEnemy integration. Full root AI/actions/combat, effects, decoded audio,
inventory and cross-room saves remain pending, and New Game stays disabled.
The final Windows build passes all41 headless checks and refreshes the ZIP.
A fresh packaged dummy-SDL profile passes241 runtime ticks,240 rendered
frames,53 movement ticks,10 focus-blocked ticks, main-menu return and INI
creation. The final205-room scan passes after the source-review correction.

Native draw-time npCalcMorphing/npTransform now handles original zombie mouth
shapes after the collision cache update. Ordinary skin coordinates are prepared
once using source translation-only bind conversion; raw retail models stay
unchanged and the source/target pair is retained separately. Drawing creates
private morphed vertex streams, preserves source object-position arithmetic
and unclamped shape values, skips evalflags8/missing models, and retains the
count-mismatch position update. Identical XYZ preserves source normals/W;
changed XYZ interpolates positions/normals and retains target W lanes as the
source VU instructions do. Drawing never refreshes collision matrices.

Initialized zombie works now compose through the cached source drawer,
including primary1000 and linked cache ownership. The original parent
stflg1000000 gate suppresses linked bodies. st_flg20 bypasses mouth morphing;
unsupported mdflg4 drawing recalculation still requires its scheduler.
The full root AI/effects/combat, lighting and clipping policy, audio, inventory
and cross-room saves remain separate, and New Game stays disabled.

The retail diagnostic draws30 actual mouth-shape frames after paired native
SetMtn/CalcEnemy updates; all30 change same-cache geometry while preserving
raw assets, joint poses and cached matrices. Existing205-room decoding and
4,800 exact zero-drift foot checks pass. Source review found no remaining
Critical/Important/Minor issues, including runtime composition gates.
All42 headless checks pass, the Windows build/ZIP are refreshed, and a fresh
packaged dummy-SDL smoke passes241 ticks,240 rendered frames,53 movement
ticks,10 focus-blocked ticks, main-menu return and INI creation. The final
205-room scan passes with the completed runtime composition integration.

Native KamiEffect and WaterEffect now route all original event-table rows into
the source 512-slot effect pool. Blood births preserve linked joint11, both
three-slot particle groups, colors, normalized direction and partial allocation
failure. Water steps preserve duplicate events, cached joint offsets, ripple
before splash order, original scales and static-then-dynamic ground/water
queries. Birth consumes no RNG; particle playback initializes later in the
source. SetMtn uses the incoming frame and rolls back the immutable effect pool
and damage fields on a later failure. Room changes clear effect slots/dynamic
floors while retaining the global EF template.

The final source review identified scratch retention and signed16 splash yaw
differences; both have RED-to-GREEN regressions. The triangle query also retains
the source rounded py+h comparison. EF template angles/floor/model version use
their original widths, with effect-work widening/narrowing at allocation.
All45 headless checks pass and the Windows executable/ZIP are refreshed.
The retail inspector now uses actual native routers in its paired motion/cache
loop, with separate blood/particle and duplicate step birth probes against
original cached models and room ground/water data.

These are effect allocations and metadata. Particle updates, effect texture
binding and visible playback remain pending, together with full enemy actions
and combat, decoded audio, inventory and cross-room saves. Step1 remains active
and New Game stays disabled.

Final verification after source corrections:45/45 headless checks;205retail
rooms,3200native routing calls,10blood/particle probes,10duplicate step probes,
4800exact zero-drift paths and30changed same-cache mouth draws. A fresh packaged
dummy-SDL profile passes241ticks/240renders/53movement ticks,10focus-blocked
ticks, main-menu return and INI creation. No desktop windows were opened.

Native MV02 close pursuit now includes source startup, steering, acceleration,
fast-walk selection, disengagement, wall recovery and transition/voice tails.
ChgWalkMtn preserves paired selection, same-motion retention and the original
double fixed-point shift; FastWalkCheck retains the active-root exclusions.
Updates stage motion/scalars/RNG/requests until consumed context validates,
without copying models or poses. USA ELF confirms the legacy phase shift.

All48 headless checks pass. The final205-room scan adds180isolated pursuit
state updates and10literal phase-boundary probes using original paired motion
banks. The existing motion/cache/effect/mouth probes still pass. Fresh packaged
dummy-SDL smoke passes241ticks,240renders,53movement ticks,10focus-blocked
ticks, main-menu return and INI creation; the executable/ZIP are refreshed.
Source review found no Critical/Important discrepancies; nonzero same-motion
frame/blend retention coverage is a deferred Minor.

These probes verify pursuit state and metadata. The legacy remap can reach the
terminal motion key, whose sampling must be resolved before controller
integration. Full enemy actions/combat, effect playback, decoded audio,
inventory and cross-room saves remain pending. Step1 stays active and New Game
stays disabled.

The walking-remap terminal sampling dependency is now resolved for the zombie
path. Source sampling reaches the relocated MDATA pointer table at the final
angle channel, so the PC uses a bounded next-loop sample there while retaining
the original frame/counter/boundary and effect/foot-fix callback timing. This
is an explicit compatibility fix to an original out-of-range read.
All10retail boundaries run paired SetMtn and CalcEnemy. Their local poses match
frame0 sampling; world translations retain source foot correction using the
incoming terminal frame, so caches are validated independently rather than
incorrectly required to match a frame0 event sequence.

Final verification:49/49headless checks,205retail rooms,180pursuit state ticks,
10paired terminal sampling/cache boundaries and all prior retail probes pass.
The comparator review finding has a retail RED-to-GREEN regression. Fresh
packaged dummy-SDL smoke passes241ticks/240renders/53movement ticks,
10focus-blocked ticks, menu return and INI creation. Executable/ZIP refreshed.
Deferred Minor review findings: mirrored Hokan terminal regression coverage and
the scenario actor caller's stricter frame guard. Full Step1 remains active.

Native MV05 lunge now starts paired122/322 motions, steers through the source
window, captures only on frames8..12, enters recovery on a frame12 miss and
returns both works to walking at the last authored frame. Captures retain
original character85/285 or121/321 pairs, blend values, enemy flags and packed
player {4,0,0,4}/enemy {2,0,0,2} modes. Motion/scalars, linked state, player grab
flags/modes and sound requests commit only after consumed context validates.
No assets or poses are copied by the action tick.

EatCheck implements all three source modes using cached player joint3/root
joint10, actor-position cone, strict distance, exact Y and health gates, plus
transformed sphere/capsule contact with the original float projection/order.
Cached Y is overwritten for planar distance, so unrelated cached Y is ignored.
The final source review caught eager joint0 validation despite skipped steering;
its flag80 regression failed before the correction and passes afterward.

Retail probes advance complete missed-lunge recovery and eight actual paired
motion/cache frames before successful capture, using Claire's original bind
hierarchy positioned at the attack origin. This is stronger state/animation
dependency evidence, but interactive player animation and grab damage/playback
remain unverified and required. Root dispatch, complete combat, effect playback,
decoded audio, inventory and cross-room saves remain pending. New Game stays
disabled and the full Step1 goal stays active.

Final verification:51/51headless checks;205retail rooms;490native lunge updates
and10cached Claire capture transitions after eight original motion/cache frames.
Prior pursuit/terminal/effect/foot/mouth/corridor probes pass. Fresh packaged
dummy-SDL smoke passes241ticks/240renders/53movement ticks,10focus-blocked
ticks, menu return and INI creation; the executable/ZIP are refreshed.
Deferred Minor review findings: exact two-facing angular boundary tests and a
retail assertion that accepts either grab flag rather than requiring both.
The native implementation sets both, with an exact flags regression in unit
coverage. These diagnostic limits do not complete the combat/playable slice.

The native player-side held/lunge grab controller now ports complete PlyDG00
states0..8: five-tick facing alignment, character/facing clip selection,
escape and fatal-release cleanup, directional input clearing, retained base
motion-bank restoration, and original voice/sound requests. It keeps source
signed16 waxp separate from signed32 ayp and the authored angular speed.
Player extended status flags remain distinct from ordinary work flags.

EnemyPushChk now preserves the original128-slot scan, exclusions, strict3D
distance, facing/knockback modes, probabilistic early return before distance,
and the source write to the attacker's linked body. Player/enemy scalars,
queues, input and RNG commit only after all consumed context validates.
Retail dependency probes pass2340player-grab states and2260original Claire
motion/cache samples, with20escapes and20fatal releases across both facings
and all10graveyard roots. These probes apply the original clips to Claire's
hierarchy; enemy NG00 damage and PlayerLink coupling remain separate required
work. Standing-grab PlyDG01 rejects explicitly when its dispatch is enabled.
Full combat, audio playback, inventory and cross-room saves remain pending;
New Game stays disabled and Step1 remains in progress.

Final verification:53/53headless checks and205retail rooms pass with the new
player-grab probes and all previous probes retained. Fresh packaged dummy-SDL
smoke passes241ticks/240renders/53movement ticks/10focus-blocked ticks,
main-menu return and INI creation; executable/ZIP refreshed. Fresh source review
found no Critical/Important production defects. Deferred Minor coverage:
combined push plus failed-release rollback and negative-angle rounding. Existing
separate rollback and positive/exact180 alignment checks pass.

Native en01.c::NG00 now implements the complete held/lunge enemy-grab callback:
paired startup/character motions, authored frame25/60 bite damage and personality
bonus, frame20 timer initialization, pressed-input LeverCheck escape, health<0
fatal classification, recoil, wall/cliff recovery, cap row4 flight metadata,
cached-root relocation and all authored recovery states. Source same-motion
counter retention and optional null body remain intact. Root/body/player/cap
scalars, motion changes and sound requests commit after consumed context
validates, including late cliff failures after recoil and staged cap flight.

The native hitchkl line query covers all original wall types0..7 with original
static/dynamic order, clipped endpoint, last hit, raw plane normal, one-sided
polygons, strict end exclusion and raw signed dimensions. It preserves source
type7's unusual negative-X conditional. Native compatibility completes the
type2/3 octagonal top before querying; upstream reads uninitialized vertices
during its fill loop. This defines the top geometry without reproducing those
undefined stack reads.

Retail dependency probes now join real lunge captures through NG00 bite damage,
pressed-input escape or fatal release, PlyDG00 and paired original motion/cache
updates. Initial scan passes3890joined state updates,40authored bites,
20input escapes,20fatal releases,3380Claire and7700enemy motion/cache samples
across both facing branches and all10graveyard roots. Enemy Nage routing,
standing grab callbacks and PlayerLink/world positioning remain separate
required work; these checks do not establish complete interactive combat.
Step1/New Game remain gated pending the complete gameplay/audio/inventory/save
slice and its final acceptance checks.

Final verification:55/55headless checks, all205retail rooms,3890joined grab
updates,40bites,20pressed escapes and20fatal releases pass. Fresh packaged
dummy-SDL smoke passes241ticks/240renders/53movement ticks/10focus-blocked
ticks, menu return and INI persistence; executable/ZIP refreshed. The first full
build encountered a permission-denied rename in the asset-recovery test's
unique scratch folder. Its independent rerun and the full build rerun pass;
the responsible process remains unverified. Both results are retained locally.
Source review found no Critical/Important production defects. Deferred Minor
coverage: polygon endpoint/corner priority/near-parallel threshold fixtures and
sound/RNG rollback after late fatal-cache failure. Source paths reviewed as
correct; these cases lack focused regression protection. Step1 remains active.

Native PlayerControl now routes standing-grab mode2=1 to complete PlyDG01
states0..5. It preserves delayed frame11 startup versus immediate state4,
character/facing clip offsets, Claire's unwrapped frame546 yaw, frame19 voice
requests, and source state2 without fallthrough. Frame0 release reads cached
player root XZ, updates the separate Player EXP0 movement anchor, restores
the base bank, clears original input/flags, and immediately samples motion42
with source Hokan rate49152/count8. This call retains count8 and the prior
model cache; ordinary subsequent model work remains the caller's responsibility.
Scalars, queues, shared scratch and sampled pose commit after validation.

Original Claire dependency probes cover clips401/402/404/405 and base42 across
both facings, both startup paths and all10graveyard roots, retaining flag1000
model placement while held and consuming the authored release loop boundary.
These are isolated complete player callbacks; NG02 enemy timing/head-cutting,
NG01, PlayerLink/world alignment and root scheduling still require porting.
Decoded audio, inventory and versioned cross-room saves remain pending;
New Game stays disabled and the full Step1 goal remains active.

Standing-grab source review found an Important release-validation gap: a
truncated cached-work layout or excessive selected range could reach cleanup.
Both cases have observed failing regressions and pass with complete pose/cache
size and selected-range validation. The full56-test build passes after this
correction. Deferred Minor coverage: malformed motion42 attribute failure
after successful pose sampling; existing staging appears correct but lacks
a focused regression for that late failure.

Final corrected verification:56/56headless checks,205retail rooms,
4480standing player states/4440original Claire samples/40cached-root releases,
and all previous joined bite/escape/fatal/corridor probes pass. Fresh packaged
dummy-SDL smoke passes241ticks/240renders/53moves/10focus-blocked ticks,
main-menu return and INI creation. Executable/ZIP refreshed; Step1 stays active.

Native en01.c::NG01 now implements complete failed-grab recovery states0..4:
7/207 startup at authored frame16 with retained same-motion counters,
frames18..30 recoil using original double constants, Zulzul/Gakeoti wall/cliff
checks, 12/212 frame7 ordinary recovery, frame20 sound requests, five-step
correction, paired walking stop or authored cliff death. It preserves NG01's
flags and avoids adding NG00 cap-flight/Z100/flag40 side effects. Small scalar
Work staging shares the existing native wall/cliff services; late invalid
context rolls back root/body, sound queues and RNG.

Original graveyard dependency probes advance both animation works and source
caches through complete NG01, accepting the original ordinary, wall or cliff
branches selected by the actual room data. The source root/Nage dispatcher
remains a separate caller dependency. NG02/head cutting/effects, PlayerLink,
complete combat, decoded audio, inventory and versioned cross-room saves
remain pending; New Game stays disabled and Step1 remains in progress.

Final NG01 verification:57/57headless checks;205original rooms;436failed-grab
states,852paired original motion/cache samples and10recoveries (no retail
cliff deaths). All previous bite/escape/fatal/standing/corridor probes retained.
Fresh packaged dummy-SDL smoke passes241ticks/240renders/53moves/
10focus-blocked ticks/main-menu return/INI creation; executable/ZIP refreshed.
Source review found no Critical/Important defects. Deferred Minor coverage:
consumed missing motion-bank rollback after staged motions or sound. The first
full build hit the recurring permission-denied recovery scratch-folder rename;
an independent core check and full rerun pass, with both results preserved.
The responsible process is unverified; controlled sharing-lock recovery
coverage and bounded handling are the next foundation correction. Step1 active.

The Windows asset-cache sharing correction is now verified and committed:
controlled transient/persistent directory-lock tests,57/57checks,205rooms,
native real ISO import/reimport and fresh packaged headless smoke pass.

Native en01.c::NeckBloodEffect and ExpHeadEffect now create complete original
birth state through the existing512-slot effect pool. Blood emits eight joint10
links with original directions, types, counts and texture remapping. Fragments
transform the original cached joint10 point and consume four source random
scale rolls before allocation, even when the pool is full. Template retention,
first-free allocation and reserved slots match the existing source services.
Model-texture ownership follows the shared cache separately from effect linkage.
Pool and RNG commit after validation; source model assets and caches stay intact.

Focused source tests cover all256model versions, source asymmetric sine-table
directions, translated/rotated joints, full/partial allocation, unused context,
shared ownership and malformed consumed-cache rollback. Original graveyard
probes cover ten root/body pairs and240births. These are creation callbacks;
particle playback/drawing and NG02 timing/head-cut state remain separate work.
Complete encounter routing/combat, decoded audio, inventory and versioned saves
are still required for Step1; New Game remains disabled.

Final head-effect verification:58/58headless checks,205original rooms,
ten root/body birth probes/240effects and all prior grab/corridor checks pass.
Fresh packaged dummy-SDL smoke passes241ticks/240renders/53moves/
10focus-blocked ticks/main-menu return/INI creation. EXE/ZIP refreshed.
Source review found no material defects. Deferred Minor: the effect file's
older header says birth consumes no RNG; fragment creation correctly consumes
four rolls, explicitly preserved by its local source comment and these tests.

Native en01.c::NG02 now implements complete standing-grab states0..3: paired
9/209 startup from prior15frame0 or otherframe10; frame10ct0=80; frame27
personality bonus+5damage clamped to0; source motion-end/timeout release into
36/236 for Claire or35/235 for every nonzero player ID. It hands player mode3=2
and control1=5 to the existing PlyDG01 callback without state3fallthrough.
Claire blood24/cut+sound4877at25 and non-Claire fragments33/cut+4869at34/
blood35 preserve source timing. CutHead sets only extended head word78=-1.
Exact authored terminal frame stops both works in modes{4,2,0,0}, clearing
root lowF/8000 only. Scalar/head/player changes commit after validation;
late consumed-bank/body failure restores effect pool and RNG as well.

Six focused checks pass. Original Claire probes join complete NG02/PlyDG01
through native source animation end flags, paired SetMtn/CalcEnemy, source
cached-root player release and immediate base42 sample:4680enemy updates,
4680Claire samples,9280paired enemy samples,40head-cut deaths/40player
releases/320neck-blood births across10roots, both facings and both startups.
Non-Claire motion35events remain literal synthetic coverage. The prior stale
effect header RNG statement is narrowed here to distinguish Kami/Water birth
from four fragment rolls. Particle playback, PlayerLink/world alignment,
complete root dispatch/weapon combat, decoded audio, inventory and versioned
saves remain required; Step1 stays active and New Game remains disabled.

Final NG02 verification:59/59headless checks,205rooms,4680enemy updates/
4680Claire samples/9280paired enemy samples,40head-cut deaths/40player
releases/320blood births. Prior grab/corridor probes remain passing.
Fresh packaged dummy-SDL smoke passes241ticks/240renders/53moves/
10focus-blocked ticks/main-menu return/INI creation; EXE/ZIP refreshed.
Source review found no material defects. Deferred Minor coverage: terminal
body validation failure after fragments/RNG; late-bank rollback is tested and
the same rollback guard covers the inspected body path. Step1 remains active.

Native PlayerLink prerequisites now retain all twelve authored Claire/Chris/
Steve XZ tables (660pairs), four character mappings including player3Chris
reuse, and checked clip/frame lookup. An independent pinned-source verifier
compares all1320float32 words. Source Dansa queries now select the first
eligible inclusive XZ area on the current signed floor, update state20000
with the original30gate, and generate uncovered temporary N/E/S/W boundaries
from actual work radius and current or remembered area. These pure services
return caller-owned walls; the pending stateful WallEx must append/use/remove
them. They do not move height or complete PlayerLink world alignment.

Focused checks cover authored samples, full Chris reuse, malformed bounds,
static/dynamic ordering, edges, ignored height, state gates, remembered-area
fallback, all four boundary coordinates, adjacent-area coverage and consumed
geometry failures. A separate north-only fallback fixture prevents reading
unused old depth. Original scans pass205rooms:57areas/570static+merged
center/edge points/430temporary boundaries (N/E/S/W86/108/134/102), and2200
Claire frames across ten initialized graveyard roots/four enemy-bank clips.
All previous NG02, grab and corridor probes remain passing. Stateful WallEx/
Wall2/PlayerLink, root encounter dispatch/combat, particle playback/drawing,
decoded audio, inventory and versioned saves remain required for Step1;
New Game remains disabled.

Final PlayerLink prerequisite verification:62/62headless checks,205original
rooms,57step areas/570static+merged points/430temporary boundaries,2200Claire
frames across10roots, all previous NG02/grab/corridor probes. Fresh packaged
dummy-SDL smoke passes241ticks/240renders/53moves/10focus-blocked ticks/
main-menu return/INI creation. EXE/ZIP refreshed; source review found no issues.
This verifies the providers; complete PlayerLink and the playable slice remain
in progress.
