# Campaign source provenance

Adapted from [aikatore/recvx-enhanced-port](https://github.com/aikatore/recvx-enhanced-port)
at commit `38d457e3ec11a3120447e3544031af9a40eccbdb` (release 0.6.8), with the
single-player portions of releases 0.6.9–0.8.0 (`8a1b6f066f9aab03618a428c575e034c2769da55`)
adapted locally, Copyright (c) 2026 aikatore and contributors, MIT; see the
[upstream license](https://github.com/aikatore/recvx-enhanced-port/blob/8a1b6f066f9aab03618a428c575e034c2769da55/LICENSE).

The associated decomp fork is pinned at
`4aac5133901da3821fe443b79d6cdd1e3b29e6da` in
`upstream/recvx-campaign-decomp`; `campaign/patches/offline.patch` carries the
offline adaptation and the selected single-player fixes from the fork through
`f30668f6fccddaa53ba3540b222a28fa09935165`. Its license and upstream AshfordFamily
attribution remain there. The old `upstream/recvx-decomp` pin is untouched.

Imported: platform and gameplay glue, renderer, movie/audio/input
systems, build helpers, tests and technical documentation. Excluded: program
icon/resource artwork, screenshots, mod/demo assets, compiled executables,
game data and proprietary SDK submodules. The F1 interface is newly composed
in code for Ashford's Halberd. No reference graphical assets are included.

Local adaptations: separate campaign build, Ashford executable/window branding,
user-data defaults, original F1 layout/style/search, dependency diagnostics and
Windows build/package workflow, and reuse of Ashford's retail ISO validation.
Validation adaptations also correct full SDL event initialization in the
binding test, native Windows command parsing in the compiler-check tool and
window screenshots during interpolated presentation.
Original internal `recvx_*` names and setting
keys are intentionally retained for compatibility and maintainability.

The documents below `campaign/docs` describe the reference version and are
retained for technical context. See `docs/campaign-backend.md` in the parent
project for the integration's actual validation status.

## Ashford offline adaptation

Online transport, server, co-op gameplay, lobby/chat/trading and router-mapping
code have been removed. The retained appearance feature operates only on the
local player. The pinned decomp receives a reproducible local patch for offline
hooks, the title menu and native timer cheats; see campaign/patches/offline.patch
in the parent checkout. Original upstream source attribution remains intact.

The launcher, halberd icon, manor illustration and synthesized ambient score
are original Ashford assets under assets/branding, outside this imported source
tree. The original F1 and in-game enhancement interfaces share settings and
Cheats actions. No reference artwork is included.


The platform prelude retains a small set of compile-time inert adapters for
legacy decomp hook callsites. They carry no session state and cannot open a
connection. Transport, sockets, server code and network dependencies are absent;
these adapters only preserve the original single-player control flow while the
pinned decomp remains maintainable.

Reliability adaptations (2026-10-24): correct the decomp's inventory-expansion
flag in ammunition lookup, preserve native puzzle/interaction ownership in
modern movement, handle push contact while run is held, filter an unavailable
vibration row, and guard presentation pacing across fast swaps/GPU waits.
Regression code selects production routines and retail data tables rather than
maintaining alternate inventory or input implementations.

Ashford weapon shortcuts (2026-10-24) add shared rebindable carried-weapon
selection and conservative animation/input ownership gates around the original
WeaponSet routine. The local decomp patch contains only the input/player hook
calls; new selection logic is in pc/glue/weapon_switch_glue.c. No graphics or
other reference-project assets were added for this feature.

The controller follow-up assigns six standard-controller combinations behind
left-stick click, adds combination capture/names/persistence, and retains base
button ownership through release. Native inventory testing also corrected the
packed-word interpretation of two-cell weapons in Ashford's shortcut scanner.

Ashford picture/preset follow-up adds a shared saturation control, an original
GLSL implementation of classic fast approximate anti-aliasing, and scoped
Original/Enhanced/Custom configuration rules. Shader and preset tests use the
production implementation. No reference-project graphical assets were added.

Upstream v0.8.0 single-player sync (Waves 1–3) brings in the remaining offline
single-player enhancements through `8a1b6f066f9aab03618a428c575e034c2769da55`
and decomp `f30668f6fccddaa53ba3540b222a28fa09935165`: 3D-only FXAA/SMAA with
16f IGN dithering/debanding, GS/interpolation performance passes, TIM2 HUD
icons, Combined fixed-camera controls, 255-slot enemies with per-slot sound
banks and custom spawn-weight randomizer, per-character appearance with Wesker
afterimage trails, Cutscene Cast & Voice-Line randomizers, Ashford Pause menu,
Title EXIT, Visual Give Item picker, procedural vector glyphs, Extra Item
Boxes/Blue Herb Planters/Antarctica Typewriter, strictly offline local BioRand
pack & In-Game Spoiler Log support, offline Outbreak File #2 character disc
import, offline multi-language & cutscene subtitle disc import, and
`mods/MOVIE/*.PSS` overrides. All online co-op, lobby, trading, relay/UPnP
networking, auto-updater, and BioRand online downloader code remain excluded.
