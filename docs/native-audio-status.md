# Native original sound assets

The native library decodes the selected Step1 sound assets and synthesizes
the original ambient sequences offline. **Runtime playback and the playable
slice remain incomplete.** New Game
is disabled; original scenario opcode148 still rejects explicitly because
its sequenced playback service is unfinished.

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

## Next required integration

Complete required native effects/reverb and source fades, validate synthesis
against original-game reference, and implement remaining active instrument
branches where required by the slice. Connect the sequenced PCM buses to
native SDL audio with separate music/voice/effects volumes, verified using the
dummy driver. Preserve original148 request timing and room fade-in behavior.
Decode required ADX music/voices separately. Then prove audio through actual
room entry and gameplay, along with combat, inventory, cross-room saves and
original-game reference acceptance.
