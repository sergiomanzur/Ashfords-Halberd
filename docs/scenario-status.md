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

The synchronous interpreter supports END, IF/ELSE/ENDIF, CK, SET, SetDoorCall
and RoomCaseNo. CK/SET preserve MSB-first flags, bank capacities and SET's six
single-bit/tail operations. CK also checks interaction/floor indices for the
callback bank. IF handling preserves the source G_Sp target cursor rather than
substituting a conventional stack. Truncated instructions, invalid branches,
invalid flag accesses and unsupported opcodes fail explicitly. An instruction
budget prevents a malformed loop from hanging the process.

SetDoorCall emits a decoded transition request. Applying `bhSetDoorDemo`'s
player/input/fade state changes, selecting the archive room by stage/room/case,
loading the destination, and preserving persistent progress are still pending.
The current subset cannot execute the complete corridor or graveyard scripts:
item checks, model/interaction activation, task scheduling, entity control and
audiovisual commands must be ported before that is possible. Unsupported
commands are not skipped.

The retail RM_0010 control script sets RoomCaseNo before SetDoorCall into
stage 0, room 2, position 0. Persistent flags choose the first-visit or return
case. RM_0020/RM_0021 contain the return door into room 1, position 1. These
records establish the candidate connected pair without inventing room links.
