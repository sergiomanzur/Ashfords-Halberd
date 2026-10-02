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
gameplay loop is not yet connected to these native updates.
`bind_room_enemies` now constructs the original 128 runtime slots from active
authored records, retains complete immutable model banks, aliases the primary
model, calculates floor and assigns the regular motion banks in runtime order.
The script lookup map preserves ETTY `work_number` values, including inactive
entries, rather than inventing a map from allocation order. Binding replaces
enemy state only after validation, preserving inventory and persistent flags.
Synthetic tests cover compaction, signed yaw, ignored authored X/Z rotation,
ID-dependent model flags and failure rollback. The retail probe binds enemies
in all 205 rooms and uses the resulting corridor state for startup and its
complete event. Object/item loading and application room lifecycle remain
pending.
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
animation. Sound commands 3/4, object/item asset loading, system room-change
flags and application integration still remain to implement.

The retail RM_0010 control script sets RoomCaseNo before SetDoorCall into
stage 0, room 2, position 0. Persistent flags choose the first-visit or return
case. RM_0020/RM_0021 contain the return door into room 1, position 1. These
records establish the candidate connected pair without inventing room links.
