# Native original sound assets

> **Status clarification — 2026-10-04:** Step 1 is incomplete and New Game is
> enabled as an experimental prototype. See [current Step 1 status](step1-slice-status.md)
> and the [dated audit](project-audit-2026-10-04.md) for present defects and verification.
> The component record below is historical: its test counts, menu gates, script
> boundaries and then-pending features describe those checkpoints, not the current build.

## Historical component checkpoints

The native library decodes and synthesizes selected original sound assets.
Native ambient requests and bounded SDL output are now joined to the corridor
runtime, with an in-menu effects volume. **The complete playable slice remains
unfinished.** New Game is disabled.148 now executes its source request behavior;
the two graveyard initialization scripts stop later at47/67, so their actual
gameplay audio and full room entry remain unverified.

## Implemented

- Original MULTSPQ IDX key lookup and SPQ records, including bank/type identity.
- AFS zero/space signature variants. The USA sound archive uses `AFS `;
  pinned CRI `adx_fs.c` checks the first three signature bytes. Existing extent,
  overlap, count and unknown-signature checks are preserved.
- Bounded SCEI HD/BD transport and declared sizes. Real wrappers include up
  to63 bytes of padding, excluded from the decoded header/body.
- Original wave rates, PS-ADPCM PCM16 samples and encoded loop boundaries.
  Terminal filler is excluded. Predictor history and signed nibble order are
  retained; unsupported headers reject explicitly.
- Indexed instrument layers, sample sets, sparse programs, split records,
  ADSR words and complete parameter blocks for subsequent native synthesis.
- Aggregate expansion caps:16,777,216 PCM frames per bank/sample and1,000,000
  events per SQ file, checked before growing decoded vectors. Callers can
  select tighter budgets. Duplicate wave/sequence indices remain valid within
  the budget; aliases cannot multiply a small disk asset into unbounded PCM
  or event allocations.
- All indexed MIDI blocks in SQ, including source running status, one-byte
  note-off, high-bit delta omission, tempo, pitch and loop control events.
  Loop controls are preserved as events; the separate native playback clock
  executes the selected ambient sequences' endless loops.
- `AshfordAudioInspect` exports private WAV/HD/BD and sample/event TSVs without
  opening an audio device. It requires a fresh output directory and decodes
  all selected records before creating output. It is a developer tool and is
  not added to the player-facing menu or game distribution.

## Headless verification

Synthetic literal format/codec tests and console inspector integration cover
malformed extents, unknown headers/events/compression, sparse tables, broken
instrument references, original sequence timing and output preservation.
No retail bytes are embedded in tests or distributed artifacts.

Private original asset inspection uses the production decoder:

| Sound key | Original room | Decoded waves | Sequence events |
| --- | --- | ---: | ---: |
|10|Claire corridor|45|798|
|20|Claire graveyard case0|88|959|
|21|Claire graveyard case1|91|959|

Both graveyard variants retain bank3's seven original waves and both ambient
sequence indices. The common instrument bank also decodes11 original waves.
This establishes actual sound asset access, not runtime sound playback.

Independent PCM validation uses development-only vgmstream r2117 CLI, installed
by `tools/setup-audio-oracle.ps1` into ignored `local/tools`. Its Windows x64
archive SHA256 is checked before extraction:
`6c4a8a3813864fefed081bbd337dbc0ad93bf88e0b92f5db98d7ab258b22dc6c`.
The native rational predictor and vgmstream's floating predictor may differ
by at most one PCM16 unit; the verifier checks the entire native sample prefix,
rate and length against independently decoded original HD/BD. Native samples
are never trimmed to obtain a match. Results remain private under the export's
`oracle/verification.json`.

The selected original exports were compared in full:224 nonzero waves,
6,141,884 PCM frames, maximum difference **zero** across all three sound keys.
Private log: `local/audio-oracle-retail.log`. This proves offline sample
decoding; it does not prove console hardware synthesis or audible playback.

After the review's aggregate-allocation fix, the full build/package passed
77/77 headless tests. All224 original sample exports were regenerated and
compared again with zero difference (`local/audio-budget-oracle-final.log`).
The fresh packaged dummy-SDL smoke verified241 ticks/240 renders/53 movement
ticks, focus blocking, return to menu and profile INI creation. Original room
entry probes still explicitly report148 for both graveyard variants.

Example (console only; use an unused output path):

```powershell
./build/windows/AshfordAudioInspect.exe `
  local/cache-sharing-real-import/assets/MULTSPQ1.AFS `
  local/cache-sharing-real-import/assets/MULTSPQ1.IDX 20 local/audio-room20
./tools/setup-audio-oracle.ps1
./.venv/Scripts/python.exe tests/audio_oracle_check.py `
  local/tools/vgmstream-r2117/vgmstream-cli.exe local/audio-room20
```

## Provenance and adaptation

Game pack semantics derive from pinned MIT recvx-decomp
`12a05ddfeef521d1f5e9a4082a182e788329135f`, `sdfunc.c`/`ps2_sg_sd.c`.
The separately written native implementations use format/codec information
from these primary projects; they are altered adaptations, not their original
source code:

- [vgmstream HD/BD and PS-ADPCM](https://github.com/vgmstream/vgmstream/tree/7dc938fa2f210943b37c7b6511852b516ef432ab/src),
  commit `7dc938fa2f210943b37c7b6511852b516ef432ab`.
- [VGMTrans SonyPS2](https://github.com/vgmtrans/vgmtrans/tree/33673176917c122627d4c317b69130c4ffbdffcc/src/main/formats/SonyPS2),
  commit `33673176917c122627d4c317b69130c4ffbdffcc`.

Their complete notices are preserved in `licenses/` and the Windows package.
No vgmstream/VGMTrans runtime dependency, PS2 SDK or IOP driver execution is
introduced. `MANATEE.DRV` is not executed by the native port.

## Native sequence synthesis

`SceSequencer` executes the selected ambient SQ loops using controller99's
start/end marker, controller6's loop number and controller38's endless count.
It jumps to the event after the start number without resetting wall time,
channel state, tempo or released sample tails. Integer rational deadlines
retain fractional frames across tempo changes and loop repetitions. Events
occur at the first48kHz frame at or after their timestamp, in source order.
Finite/nested loop forms explicitly reject until their semantics are proven.

`SoundEnvelope` and `SampleVoice` produce native PCM from the original ADSR
words, four-point interpolation, Q12 pitch, sample rates and exclusive sample
loops. The independently written envelope and mathematical interpolation
coefficients use pinned [primary SPU documentation](https://psx-spx.consoledev.net/soundprocessingunitspu/)
(psx-spx6184740310ee418c24e069e7e118ba94a367ee34); the PS2 rate/parameter
layout is informed by the pinned VGMTrans reference above. **This is a native
synthesis candidate, not proof of PS2 hardware output parity.** Fast
exponential rates, driver gain/pan behavior and loop timing still require
original-game reference acceptance. No GPL emulator implementation is copied,
compiled or added to the runtime.

`SceMixer` binds original indexed programs/splits/velocity layers, transpose
and detune, multiplicative gain and additive stereo pan. Channel volume,
expression and pan changes affect active voices. Direct and effect sends
remain four distinct int32 buses; an effect-only sample cannot appear in the
direct bus. Banks are held by shared immutable ownership, never pointers into
a caller-owned temporary. Callers must not mutate a shared bank during playback.
Limits are48 voices,48000 frames per block and65536 scheduled events per block;
overflow/invalid indices/parameters and unsupported active branches reject.
Pitch controllers, active modulation/key-follow/groups/crossfade/velocity
curves and non-core0 sends remain explicit unsupported states. Oversubscribed
voices report an error; original voice stealing is still required where used.

Focused headless tests verify rational timing/tempo changes, loop guards,
fractional deadlines, block partition invariance, envelope phases, signed
interpolation vectors, sample-loop history, wave rate/octave/detune, note-off,
live volume/pan changes, distinct sends, sparse/malformed instruments and caps.

Private production-library audit `local/audio_mixer_retail_audit.cpp` decoded
both original bank3 sequences in sound keys20/21 and rendered36 seconds each:
1728000 frames, two sequence repetitions, peak4 voices for sequence0 and7 for
sequence1. All four buses contain nonzero PCM. Repeating each render with
different block partitions produced identical frames. The four private
diagnostic WAVs contain dry L/R and effect-send L/R,48kHz PCM16; they are not a
final reverberated mix and no audio device was opened. Evidence:
`local/audio-mixer-original36.log`, `local/audio-mixer-original36/`.

The complete build/package passed80/80 headless CTests. A fresh packaged
dummy-SDL profile passed241 ticks/240 renders/53 movement ticks, focus blocking,
menu return and INI persistence. Fresh original-entry probes retained10 roots,
10 bodies/52 auxiliaries and120 corridor updates; both graveyard variants still
explicitly reject opcode148 at offset10. Evidence:
`local/native-sequence-mixer-full-build.log`,
`local/native-sequence-mixer-ui-smoke.log`, `local/audio-mixer-entry-audit.log`.

One fresh read-only review found no Critical/Important issues. Deferred Minors:
specify/enforce the mixer's status after a rendering exception (the clock can
already have advanced); add independent nonzero-fraction interpolation vectors
and a regression with retained channel state/release tails spanning a sequence
loop. Current tests and original-asset proofs do not establish those additional
independent checks or hardware parity.

## Room sound binding, fades and Hall candidate

`RoomSoundAssets` retains separate16-index program, shot and sequence banks
with immutable ownership. Duplicate type/bank identities and multiple
environments reject before expansion. One aggregate PCM/event budget covers
the whole pack, including aliased indices decoded by the existing readers;
remaining budgets are passed to each decoder before allocating samples/events.
The16 signed `RM_SNDENV` fields exclude transport padding; an absent environment
remains distinguishable from an authored zero-depth environment.

`MidiVolumeFade` follows matching `sdc.c` manager arithmetic, rather than
interpreting the source timer as milliseconds. Timer120 uses21 manager ticks;
400 uses60 and250 uses45. Completion occurs on the following tick after the
counter reaches zero. Float volume is truncated before the signed driver
conversion: source-127 maps to driver1 and requests a stop only at fade-out
completion. `RoomEffectDepth` follows `sdSysServer`'s16-tick integer ramp,
including interrupted ramps, signed depths and exact final targets.

The high-level effect wrappers are inert in the pinned game source, but
`sdSysServer` actually requests Hall mode5 via `SdrSetRev`. The independent
`HallReverb` implements the documented Hall reflections, four comb terms and
two all-pass stages with22256 PCM16 feedback words, signed Q15 products,
saturation and39-tap input/output rate conversion between48kHz and24kHz.
Depth scales only the wet output; zero depth retains feedback/filter history.
Dry buses use widened arithmetic before final PCM16 clipping. Reset clears
all feedback/filter/decimation state; a block cannot exceed48000 frames.

Hardware mathematical coefficients and formulas use the pinned primary SPU
documentation above. PS2 mode5, buffer size and address mapping were confirmed
against [open ps2sdk effect data](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/sound/libsd/src/effect.c),
commit `ac92a9f657d2e531dd8f060250b07f2a5ac6dea5`. No SDK implementation is
copied or compiled. **Paired-channel24kHz processing, filter alignment and
intermediate rounding remain a native DSP candidate requiring PS2 reference
acceptance.** Passing these tests does not establish bit-exact hardware parity.

New literal tests cover aggregate caps/identity/environment bounds, source
fade quantization/completion, depth endpoints, independent FIR impulse vectors,
delayed cross-channel Hall tails, extreme buses, exact odd-sized block
partitions/wraparound, zero-depth retained state and reset.

Private production-library audit `local/audio_room_services_retail_audit.cpp`
decoded keys10/20/21 through the new pack binding, confirming authored depths
16/0/0 and respectively944692/2592940/2604252 PCM frames across their typed
banks. Both original ambient sequences in both graveyard cases then passed
36-second joined mixer/fade/depth/Hall checks:1728000 frames per sequence,
two loops, nonzero direct output, exact PCM across17/783-frame partitions and
clean reset. A separate depth16 override produced nonzero wet differences;
that override is explicitly a stress candidate, not authored graveyard audio.
The audit applies linear driver-volume gain as a synthesis candidate; original
driver gain curves remain subject to reference acceptance. No audio device or
WAV player was opened. Evidence: `local/audio-room-services-original36.log`.

The complete build/package passed83/83 headless CTests. A fresh packaged
dummy-SDL profile passed241 ticks/240 renders/53 movement ticks, ten focus
blocks, menu return and INI persistence. The current actual-entry probe still
rejects148 at offset10 in both graveyard cases, preserving the explicit
runtime boundary; it also verified ten roots/ten bodies/52 auxiliaries and120
corridor updates. The ZIP contains only the EXE, README and four license notices.
Evidence: `local/native-room-audio-services-full-build.log`,
`local/native-room-audio-services-ui-smoke.log`,
`local/room-audio-services-entry-audit.log`.

One fresh read-only review found no Critical/Important issues. Deferred Minor:
compare the subsequent tail against an identically primed untouched processor
after oversized rejection or empty rendering. Existing assertions prove the
error/empty result; state preservation is visible in the implementation but
does not yet have that independent regression.

## Original request runtime and native SDL output

`BackgroundAudioState` keeps source `CurrentBgSeNo`/`ReqFadeBgSe` as values;
scenario copies and destination validation never share a mutable mixer/device.
Native148 reads the original slot/word and schedules a request;146 schedules
a stop. Raw-code comparison remains distinct from the forced bank3 code.
Ambient command3 sets initialization, matching empty command4 leaves it active,
and command1 emits source120 fade-in playback after room assets resolve.
Command2 schedules source400 fade-outs for requested stops. Batches are bounded
to32 actions and reject without replacing live slots on validation failure.
These are the ambient components of the source commands; ADX/shot/door owners
are explicitly unfinished, not implemented as silent substitutes.

`RoomAudioPlayback` owns two independent native mixers/fades and one Hall stage.
The current native60Hz cadence produces800 stereo PCM16 frames per tick at48kHz.
Each slot applies the candidate driver gain before combining dry/effect buses;
an effects percentage scales the final output. Zero effects volume preserves
sequence/envelope/filter state. A conservative sum of slot peak counts bounds
the shared voice budget to48; original shared scheduling/stealing still needs
its required integration. Rendering failures stop the owner and require
reconstruction, preventing continuation after partially advanced mixer state.

`read_room_sound_assets` uses the source key `stage*1000+room*10+case`, reads only
the selected validated AFS extent, and binds immutable assets. The room loader
resolves the post-script sound case, consumes/resets it, validates ambient
commands and constructs the replacement before committing a room transition.
Requested ambient audio without a resolver rejects explicitly.

`RuntimeView` owns the native playback/device. SDL receives48kHz stereoPCM16
through a queue capped at half a second, with explicit initialization/queue
errors. Preview exit, focus loss and errors pause/clear queued PCM; owner
destruction closes the device and releases its audio subsystem reference.
Scenario evaluation never calls SDL. **Corridor entry currently requests no
ambient slot, so its packaged smoke proves connected output/lifetime, not
audible original graveyard gameplay.**

Effects volume is implemented under main-menu Options and persists atomically
as `[Audio] effects_volume=0..100`. Bad values preserve valid settings/default
the affected key; invalid writes reject. The actual Options action and a second
process launch pass headless checks. Any valid integer volume advances to the
next ten-point menu step, with100 wrapping to mute. Regressions exercise saved
95/99 values and a full cycle from1, including persistence after every action.
Music/voice controls await actual ADX
playback. No separate configuration launcher was added.

The complete build/package passed87/87 headless tests, including literal source
requests, two-slot PCM/fades/stops/mute, dummy device consumption/queue limits,
post-script resolver/failure preservation, effects INI and actual menu action.
Evidence: `local/native-room-audio-runtime-full-build.log`.
After the review fix, the complete suite again passed87/87; final packaged
dummy smoke generated192800 frames, submitted184000 and left the queue empty.
Evidence: `local/native-room-audio-runtime-final-build.log` and
`local/native-room-audio-runtime-final-ui-smoke.log`.

The production entry probe with original RDX/player/sound assets now passes148
and reaches **47 at110** in graveyard case0 and **67 at48** in case1. It retains
120 corridor updates/60 normal-anchor changes and ten roots/ten bodies/52
auxiliaries. These next operations are not skipped. Evidence:
`local/room-audio-runtime-entry-audit.log`.

Private `local/room_audio_instruction_retail_audit.cpp` separately executes the
actual four-byte148 at offset10 from each original initialization script.
Both request slot0/code0x300. Their isolated requests, bound to original sound
keys20/21, render36s each:1728000 frames, one active slot,1727066 nonzero frames,
exact mute-clock continuation and1728000 frames transferred to a paused dummy
device with bounded/cleared queue. **This is an isolated instruction/audio
diagnostic, not successful room initialization or gameplay acceptance.** The
separate output unit test verifies actual dummy consumption. Evidence:
`local/room-audio-instruction-original36.log`.

One fresh read-only review inspected all five runtime focus areas. Its Important
finding, valid non-ten-step volumes trapping the menu above100, received a
reproducing RED test and a GREEN fix. Deferred Minor: compare subsequent PCM
against an untouched player after invalid-batch rejection; current assertions
check slot count and the implementation preflights the whole batch. Original
bank reuse, additional audio categories, full graveyard entry/gameplay and
hardware reference acceptance remain outside this increment's completion.

## Next required integration

Validate the native synthesis/effects candidate against original-game reference
and implement remaining active instrument branches where required by the slice.
Native room effects and entry controls now handle47/67/145/34 and common23/46.
Actual initializers require the common motion-selection path/companion and
opcode139's original computer-event consumer; see native-room-entry-status.md.
Complete actual graveyard entry/gameplay, original bank3 reuse and room-change
fade orchestration.
Integrate shot/footstep/enemy sounds and decode required ADX music/voices with
working separate controls. Then prove audio through actual two-room gameplay,
along with combat, inventory, cross-room saves and original-game acceptance.
