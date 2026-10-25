# Native movement and camera-area status

> **Status clarification — 2026-10-04:** Step 1 is incomplete and New Game is
> enabled as an experimental prototype. See [current Step 1 status](step1-slice-status.md)
> and the [dated audit](project-audit-2026-10-04.md) for present defects and verification.
> The component record below is historical: its test counts, menu gates, script
> boundaries and then-pending features describe those checkpoints, not the current build.

## Historical component checkpoints

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
quick turning, stairs, pushing and enemy collision still require their player
states. Healthy unarmed transition posing now retains the current joint pose
between ticks, samples the pre-advancement frame, uses the original fixed
blend rate while the blend count is nonzero, and preserves Fast root-only
translation versus Normal/Slow per-joint translation. Fast transitions use
the original alternate angle interpolation on shoulder joints 7/11, including
ZYX/YZX conversion and integer truncation. Shared quarter-wave math retains
the PS2 sine table's 16383 reflection. Synthetic tests cover a coupled-angle
wrap crossing, the shoulder-index selection, fractional Slow positions and
invalid sampled frames.

`AshfordPlayerInspect SYSTEM.AFS 10 --movement-check RDX_LNK.AFS room-index`
runs 660 ticks, poses the retail actor every tick and renders every 60th frame
fully headlessly without writing images. It passed in retail rooms 0–2 (1,980
ticks/poses and 33 rendered frames), now with persistent transition-blended
poses seeded from the original idle frame. It starts at POS entry 0 with the authored
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

Native actor contacts now adapt `hitchk.c::bhCheckPlayer` and `bhCheckEnemies`.
Pair response preserves separate primary, status and secondary flags, collision
offsets, the player's normal-anchor fallback, inclusive vertical overlap and
strict horizontal overlap. Fixed-player response uses the original truncated
atan angle and Ninja table; ordinary response splits the separation between
player and enemy. Coincident ordinary bodies retain a zero direction, as in
the source VU normalization, while contact status flags still update.

Pool response skips self, inactive/nonphysical/hidden work and linked work with
a hidden parent. The player-only flag0x40 gate is distinct from enemy-subject
response. Every candidate is calculated from the original subject position;
multiple candidates are averaged in original work order. Late malformed data
cannot partially commit contact positions or flags.

The native player frame now computes the front probe before contacts, applies
pool contact displacement, then performs the original upper/lower wall passes.
Contact context commits only after successful wall response. Runtime supplies
enemy collision dimensions, offsets, flags and parent indices. Claire contact
dimensions are 2.5/15.5 from `PlyInfo - 1`; wall dimensions remain 3.5/16.5.
Normal door entry retains the original player primary-flag mask.

Synthetic checks exercise pair/pool gates, offset geometry, height/tangency
boundaries, coincidence, multiple contacts, hidden parents, alias-safe enemy
subjects, nonfinite/late-parent rollback, frame ordering and failed wall rollback.
Angled response checks use literal source-table coordinates and fail if the
atan angle is rounded. A runtime check verifies the actual enemy flag commit
and player secondary-flag suppression. The private retail probe verifies ten
player/zombie contacts using initialized graveyard collision geometry. This
contact dependency does not yet enable the full zombie movement/damage
controller or graveyard travel.
