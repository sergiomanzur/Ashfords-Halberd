# Native movement and camera-area status

The native core now decodes the room's 32-byte camera areas and selects cuts
using the reference rectangle/triangle tests, signed floor matching, active-cut
flag, first-match ordering and one-unit transition hysteresis. Camera-area
decoding passed on all 205 retail rooms. This does not yet implement camera
tracking, scripted cut changes or the gameplay caller's stair/floor state.

Player auxiliary records decode into bounded, finite float tables, with an
explicit terminator and up to 63 zero padding bytes. Retail player packages
10–14 each contain 19 tables; every frame of healthy walk, run and backward
motion validates against its corresponding table. Baseline tables 0–10 are
sampled at frame+1. Later action tables use different indexing and are rejected
by the baseline speed helper.

`player_add_speed` follows `pwksub.c::bhAddSpeed` and the original
`ps2_NaMath.c` quarter-wave `njSin`/`njCos` lookup convention, including its
endpoint asymmetry. It preserves height and rejects nonfinite inputs and
overflow.
The reference `ps2_NaSystem.c::Ps2SwapDBuff` waits until `Ps2_vcount >= 2`;
`vsync_func` increments that counter once per vertical-sync interrupt. Native
simulation must preserve this two-field update cadence independently of rendering.

Sixteen CTest suites pass headlessly, including scenario, room-entry, cut selection and movement
primitive suites. Tests cover triangular boundaries, floor mismatch, cut
hysteresis, malformed area records, auxiliary truncation/padding/nonfinite
values, displacement indexing and cardinal/relative movement angles.

The collision decoder reads the explicit 36-byte `ATR_WORK` layout, room height
and 32 ground heights. Geometry response adapts `hitchk.c::bhCheckWall` for
rectangles, circles, all triangle orientations, slopes and ledges. Signed
dimensions are preserved: retail room 0 includes ledges with negative height.
`bhCheckWallEx` uses prior positions for rectangle face selection and rectangle/
ledge quadrants, keeps raw heights and excludes slopes, as in the reference.
The no-hit grounded floor snap is included. Ordered responses and special wall
damage failures are explicit; dynamic walls, push-state changes and generated
step limits remain pending. Collision records and both response paths passed
100,860 native sampled probes across all 205 retail rooms. The collision suite also
passes 600 deterministic mutations with GCC library assertions enabled.

`locomotion.cpp` adapts healthy unarmed standing, turning, walking, running,
backward movement and idle variation. It retains original entry frames, turn
increments, input transitions, frame sampling/advance order and blend metadata.
The unusual raw-count clamp in `bhGetFrameNum` was confirmed against the user's
USA ELF instructions at `0x14e550`–`0x14e56c`; it is preserved deliberately.

`advance_player_world` connects displacement, smoothed front-probe offset,
upper/body collision passes, grounded height, water speed state and cut selection.
Tests verify water speed changes on the next tick, stop drift, an actual wall
response and corrected front anchor, and a continuous camera boundary crossing.
The core remains limited to baseline locomotion: injury/weapon variants,
quick turning, stairs, pushing, enemy collision and animation-transition posing
still require their player states. Blend parameters are recorded but the current
diagnostic renderer samples each selected motion without transition blending.

`AshfordPlayerInspect SYSTEM.AFS 10 --movement-check RDX_LNK.AFS room-index`
runs 660 ticks, poses the retail actor every tick and renders every 60th frame
fully headlessly without writing images. It passed in retail rooms 0–2 (1,980
ticks/poses and 33 rendered frames). It now starts at POS entry 0 with the authored
yaw and original ground snapping. Room 1 and room 2 each registered two retail
camera transitions; room 0 registered none. Collision hit totals were 360, 101
and 70 respectively. These are scripted diagnostic inputs, not a reference-game
playthrough. Original-game comparison remains pending.

`room.cpp` decodes the explicit 16-byte POS and 36-byte ETTY_WORK layouts,
preserving signed floor, parameter, rotations and hide bytes. Normal door entry
initializes position, low-16-bit yaw, floor, snapped ground height, front anchor,
water status and an authored camera candidate. Water affects the first movement
step, as in `bhInitRoomChangePlayer`. Save restoration and cinematic entry are
separate paths still to implement. Authored entity records are metadata; event
scripts determine runtime activation and behavior.

All 205 retail rooms passed native record decoding and all 600 authored position
entries passed initialization. Of these, 554 immediately match active camera
areas. The other 46 are not assigned invented cameras; script state, activation
and initial camera handling require further investigation. Both entries in each
of the first seven room records match authored camera areas.

Two-way door metadata connects RM_0010 to RM_0020/RM_0021: the corridor's door
targets stage 0, room 2, position 0, and the return door targets stage 0, room 1,
position 1. The room case is selected by scenario state. This is a candidate
connected encounter pair; transition events and encounter activation remain
pending, so the pair is not yet a playable slice.

`AshfordRoomInspect RDX_LNK.AFS report.tsv --collision-check` probes active wall
records and reports active shape counts. `--scene` retains full scene validation.

Step 1 remains incomplete. These changes provide core primitives and offline
validation; New Game remains disabled until the playable mode is integrated.
