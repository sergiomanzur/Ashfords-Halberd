# Campaign source provenance

Adapted from [aikatore/recvx-enhanced-port](https://github.com/aikatore/recvx-enhanced-port)
at commit `38d457e3ec11a3120447e3544031af9a40eccbdb` (release 0.6.8),
Copyright (c) 2026 aikatore and contributors, MIT; see the
[upstream license](https://github.com/aikatore/recvx-enhanced-port/blob/38d457e3ec11a3120447e3544031af9a40eccbdb/LICENSE).

The associated decomp fork is pinned at
`4aac5133901da3821fe443b79d6cdd1e3b29e6da` in
`upstream/recvx-campaign-decomp`. Its license and upstream AshfordFamily
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
