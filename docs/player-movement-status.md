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
overflow. Motion transitions, input state changes, simulation cadence, floor
height and wall collision remain to be integrated before this is playable.

Twelve CTest suites pass headlessly, including new cut selection and movement
primitive suites. Tests cover triangular boundaries, floor mismatch, cut
hysteresis, malformed area records, auxiliary truncation/padding/nonfinite
values, displacement indexing and cardinal/relative movement angles.

Step 1 remains incomplete. These changes provide core primitives and offline
validation; New Game remains disabled until the playable mode is integrated.
