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

The synchronous interpreter supports END, IF/ELSE/ENDIF, CK, SET, ItmCk,
ItmSetCk, InitModelSet, EtcAtariSet2, ItmAspdSet, SetDoorCall and RoomCaseNo.
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
the shared branch cursor before returning. These behaviors have headless tests;
FOR/WHILE loops and wider entity controllers are still pending.

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

SetDoorCall emits a decoded transition request. Applying `bhSetDoorDemo`'s
player/input/fade state changes, selecting the archive room by stage/room/case,
loading the destination, and preserving persistent progress are still pending.
The current subset can execute corridor initialization, but cannot yet execute
its full per-frame control or the graveyard scripts. Additional inventory checks,
event loops, entity control and audiovisual commands must be ported before
that is possible. Unsupported commands are not skipped.

The retail RM_0010 control script sets RoomCaseNo before SetDoorCall into
stage 0, room 2, position 0. Persistent flags choose the first-visit or return
case. RM_0020/RM_0021 contain the return door into room 1, position 1. These
records establish the candidate connected pair without inventing room links.
