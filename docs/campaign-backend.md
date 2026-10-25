# Campaign backend integration

The original campaign backend is adapted from recvx-enhanced-port 0.6.8,
pinned at `38d457e3ec11a3120447e3544031af9a40eccbdb`. Its campaign code and
quality-of-life systems are retained together; see
[source provenance](../campaign/PROVENANCE.md). The x64 prototype is still built
with `tools/build.ps1`. This backend is built separately as Windows x86.

**Status: the Windows x86 campaign executable builds and launches from the
retail ISO. The campaign component checks pass, including every retail PSS movie.**
The original New Game loads room RM_0000 with native actors, event tasks,
music and dialogue. The new F1 menu was inspected in the running game;
changing VSync through it persists to the campaign INI. A full campaign
playthrough, natural cross-room save/load remain
unverified. The reference feature list is not a completed-playthrough claim.

The fixed-camera opening verification also opened the original inventory and
returned to movement: 400 controlled frames with 180 distinct player positions
in a 2400-frame run. This uses the game's own inputs, not patched state.
The installed executable also passed the 2400-frame third-person opening,
recording 304 distinct player positions. Screenshots were inspected for both.

## Build

Use a local i686 POSIX GCC toolchain (the reference uses GCC, not MSVC/Clang).
The integration was prepared with WinLibs GCC 16.2.0, MinGW-w64 UCRT 14.0.0,
i686 POSIX/DWARF, from the [official WinLibs releases](https://github.com/brechtsanders/winlibs_mingw/releases).
Extract it to a path without spaces; GCC's manifest-object lookup fails when
the toolchain's real path contains spaces. Source/build directories can contain
spaces and apostrophes. No global installation or PATH changes are required.

Initialize only the campaign source, without recursively downloading SDKs:

```powershell
git submodule update --init upstream/recvx-campaign-decomp
./tools/build-campaign.ps1 -FetchHeaders -ToolchainBin 'C:/path/to/mingw32/bin'
```

The retained game source requires existing local Katana and CRI declarations:

- `upstream/recvx-campaign-decomp/include/recvx-decomp-katana/KATANA/Include/ninja.h`
- `upstream/recvx-campaign-decomp/include/recvx-decomp-cri/cri/mwlib/include/cri_adxt.h`
- `upstream/recvx-campaign-decomp/include/recvx-decomp-cri/cri/mwlib/include/cri_adxf.h`

These denote header trees, not just three standalone files. The user explicitly
authorized obtaining these dependencies on 2026-10-23. `-FetchHeaders` initializes
only the Katana and CRI submodules at the decomp's pinned revisions; it does not
fetch the original compiler or PS2 libraries beyond those contained in the two
header repositories. Without this switch the build diagnoses missing headers
before configuring dependencies. SDK material is excluded from packages.

Independent component verification does not require those SDK headers:

```powershell
./tools/build-campaign.ps1 -PlatformTestsOnly -ToolchainBin 'C:/path/to/mingw32/bin'
```

This checks platform components only and cannot build or package the campaign.
`-BuildDirectory <folder>` can reuse an already configured x86 component build
for a full campaign build, avoiding duplicate open-source dependency builds.
CMake fetches the pinned open-source dependencies listed in
`campaign/THIRD_PARTY_NOTICES.md`. Run `tests/campaign_source_tests.py` with
Python to check attribution and the no-copied-art boundary.

## Run and package

After a successful full build, run
`build/campaign-windows/campaign/pc/AshfordsHalberd.exe` and choose your own
USA SLUS-20184 ISO. A direct ISO command-line argument is also supported.
The original launcher selects the disc and starts the game; --skip-launcher bypasses it.

The verified build on this workstation reuses the component build directory:
`build/campaign-platform/campaign/pc/AshfordsHalberd.exe`.

Campaign settings and saves live in the SDL per-user preference directory's
`AshfordsHalberd/Campaign` subfolder (on Windows, normally under
`%APPDATA%`). They are separate from the prototype's `%LOCALAPPDATA%` settings
and saves. `--save-dir <folder>` explicitly overrides this location.
The campaign keeps the engine's `recvx.ini` and native save formats. Prototype
`slot0.dat` saves are not compatible and are never migrated implicitly.

F1 opens Ashford's original Field Dossier: category navigation, setting search,
contextual help and the full binding editor. Narrow windows use a section
picker. F2 opens the shared Cheats page. Online features have been removed. No reference graphical assets were
imported; retail graphics are read from your disc and optional packs must be
supplied by you.

`-Package` builds, runs tests and installs a ZIP containing the executable,
player instructions, original launcher assets and dependency licenses. It stops on any failure.
Neither a successful component test run nor a ZIP is full-campaign acceptance;
original progression, cutscenes, combat, saves and completion require gameplay
verification.

## Repeatable verification

`tools/campaign-smoke.ps1 -Executable <exe> -Iso <iso> -Mode Overlay` starts
the real runtime, captures the Field Dossier and exits at a bounded frame.
`-Mode Gameplay` uses normal button inputs to start the original game and
exercise the opening, while `-ThirdPerson` selects its optional camera.
These runs use separate files below `local/campaign-smoke`, with raw game
frames, window screenshots and logs; no progress is injected. A started game
task alone is not accepted as proof of player control.

Verification also covers the menu at 320x240 through 1920x1080, source/artwork
boundaries, native Windows command quoting, disc validation, atomic saves,
bindings (20 consecutive passes after correcting the reference test's partially
initialized event union), renderer/VU math and network protocol parsers. PE
inspection confirms an x86 GUI executable with only Windows system DLL imports.


## Offline identity update (2026-10-23)

Online transport, co-op gameplay, lobby/chat/trade systems, server targets,
router mapping dependencies and their test targets have been removed. Original
single-player appearance swapping remains. Both enhancement interfaces share
one settings table and a Cheats page. Rank tools use native game-thread requests.

The native launcher uses original generated art and an original synthesized
32-second stereo theme. The source score is tools/generate-launcher-music.py.
Windows embeds the new icon. Runtime BMP/WAV assets are loaded relative to the
executable, independent of the working directory. Source provenance and image
prompts are recorded in assets/branding/PROVENANCE.md.

The offline adaptation also patches the pinned decomp source; the patch is
retained in campaign/patches and applied idempotently during configuration.

Build prerequisites also include CMake 3.25+, Ninja and Python 3. The configure
step uses Python to apply the pinned offline decomp patch safely. A conflicting
local decomp edit stops configuration instead of being overwritten. The source
and asset contract checks can be run with Python from tests/campaign_source_tests.py,
tests/campaign_warning_tool_tests.py and tests/offline_identity_tests.py.

## Latest test build — 2026-10-23

F8 is the default rebindable FMV skip key in both control editors. A fresh press
skips through the game's normal movie fade/cleanup; presses from before the
movie are discarded. Real-time scripted scenes are outside this action.

The latest executable passed 14 component checks (all except the full-disc
movie decoder rerun), including bindings, shared menu parity, all-page panel
layout, VU paths and decomp declarations. Nine source/tool/asset checks pass.
A real retail FMV accepted F8, faded/closed its decoder, and the runtime reached
1700 frames normally. The integrated launcher also reached the original New
Game task in its isolated startup run. Full-disc movie decoding was verified
in the earlier campaign baseline; the movie decoder is unchanged here.

The Windows GUI executable embeds the new icon and imports no Winsock DLL.
Built and installed executables match SHA256:
5F7D314E81AE3BEBBA8F5E6FB4555D5044B4C9C9F1F250B8AAF51B3713553E95.
The package contains 15 allow-listed entries, including original assets and
licenses, with no retail files or obsolete network-library notices.

## Reliability build (2026-10-24)

See [the detailed reliability report](campaign-reliability-2026-10-24.md) for
reload/input corrections, virtual rumble checks and frame-time results across
three display modes. This build passes 20 non-movie component checks, nine
source/tool/asset checks and both 2400-frame opening camera-mode runs. The
full-disc movie decoder was unchanged and not rerun for this update.

## Weapon shortcuts build (2026-10-24)

The latest staged executable is `build/campaign-platform/dist-quick-switch/AshfordsHalberd.exe`;
the distributable ZIP is `build/campaign-platform/AshfordsHalberd-Campaign-Windows-x86.zip`.
See [weapon switching and verification](campaign-weapon-switching.md) for controls,
state restrictions and evidence. All 22 non-movie component checks and nine
parent source/tool/asset checks pass. The final binary completed both native
2400-frame camera-mode runs with three direct selections, cycling and unchanged
ammo. SHA256: `49813590485B211127213CD1B89241A6E92C9E5DABC3CF66B3A060672256DBD3`.

The subsequent controller-default build is staged at
`build/campaign-platform/dist-controller/AshfordsHalberd.exe`, and replaces the
same distributable ZIP. All six standard controller combinations passed native
2400-frame runs in both camera modes, alongside the 22 non-movie and nine parent
checks. It also fixes scanning next to a packed two-cell weapon. SHA256:
`3F54C8537EA3363C6A8653066FF4CDD7FF24CAC72E10A1B34ECD639C7DBF7885`.

## Picture and preset build (2026-10-24)

Latest staging: `build/campaign-platform/dist-quality/AshfordsHalberd.exe`.
The same distributable ZIP now includes saturation, optional FXAA and shared
Original/Enhanced/Custom experience presets. All 24 non-movie component checks,
nine parent checks, both menu captures and the native 2400-frame opening run with
live preset/picture changes passed. See [scope, tradeoffs and verification](campaign-picture-presets.md).
SHA256: `25AA40FAB5FEF45F122DF8B03D1E251004288EFD58D4F06B386120EC261F85B8`.
