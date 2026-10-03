# Native zombie motion mapping implementation plan

Continue Step 1 after the linked-skeleton prerequisite at `c2c1b92`.
The graveyard still rejects the unimplemented ID-1 controller. This plan
covers its motion dependency; it does not replace the enemy encounter scope.

## Confirmed source and USA executable behavior

`Motion.c::bhSetMotion` chooses Slow when work mode bit 8 or motion flag 0x20
is set, Normal for motion flag 0x10, and Fast otherwise. A nonzero blend count
selects each routine's Hokan variant. Mirror mode bit 2 uses the caller's
destination map, negates translation X and angle Y/Z, and consumes tracks in
source order. The root map is `en01_flipTree`; linked bodies use
`en01_flipTree2` relative to joint 8 of the complete model.

Work flag 4 skips destination animation. Direct Normal, Slow and both Fast
variants consume no source track for a skipped destination; NormalHokan
consumes one even when skipped. SlowHokan also skips without consuming. Work
flag 2 preserves destination angles. Fast translation is applied to the first
mapped destination before checking its work flags; other translations are
ignored in that format. Translation override mode 0x100 applies during blend.
Only FastHokan uses work flag 8 for alternate Euler conversion; NormalHokan
and SlowHokan use the ordinary angle blend regardless of that flag.

Three suspicious missing-angle assignments were verified by headless static
disassembly of the user's supplied USA executable, not by executing MIPS:

- `SetMtnNormal` at 0x12F7D0: stores at 0x12F934, 0x12F938 and 0x12F93C all
  target offset zero of the destination angle vector. Missing angles clear X
  only; Y/Z retain their prior values.
- `SetMtnSlow` at 0x1304B0: stores at 0x130768, 0x13076C and 0x130770 also
  clear only destination X.
- `SetMtnSlowHokan` at 0x1307D0: stores at 0x130B3C, 0x130B40 and 0x130B44
  clear only X of the shared `mka_ang` scratch vector at 0x47D200. Its Y/Z
  retain values from earlier channel applications, including earlier calls.

These are original behavior, not decompilation transcription errors. Fast
missing angles and NormalHokan's null-angle fallback clear all axes. Generic
raw `sample_motion` remains a decoder/sampler: it should keep absent channels
absent. Stateful application must implement these branch-specific rules.

## Implementation sequence

1. Add failing synthetic tests for destination remapping, mirrored axes,
   body joint offset, skipped-track consumption, angle retention and blend
   scratch persistence. Test malformed maps without mutating caller state.
2. Implement bounded native channel application with explicit work flags and
   immutable motion/model inputs. Preserve original map order and validate all
   destination indices, track/frame bounds and finite values.
3. Retain shared blend-angle scratch in the native scenario state so copied
   runtime/save state cannot retain console pointers or cross-profile globals.
4. Use the application helper in the existing corridor actor controller and
   verify its original lifecycle. Keep unsupported controllers explicit.
5. Probe real graveyard full/body motion pairs, including both model versions,
   and headless corridor rendering. Run full tests, package smoke and review.

After this dependency, implement original body subtree selection, remaining
model flags and complete zombie initialization, then movement, attack/damage
and the required subplayer controller. Application travel remains disabled
until those paths validate; diagnostic full-model poses are not gameplay.

## Verified implementation checkpoint

Implemented bounded channel application, shared scenario scratch and corridor/
player integration. Missing-channel integration regressions failed before the
replacement and passed afterward. All 19 CTest suites passed headlessly, and
the packaged real-asset smoke rendered 240 frames with movement, focus blocking
and menu return. Retail probes checked all 205 rooms, 480 full/body graveyard
applications, 30 skeleton samples, 180 linked placements and 960 corridor ticks.
Review found an ordering error in the diagnostic's startup-motion presence
checks; moving those checks before every dereference resolved it, and the
retail probe passed again. Actual travel still fails on room 2 actor 1 mode 0.
