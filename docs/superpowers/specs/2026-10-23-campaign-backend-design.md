# Ashford's Halberd campaign backend

The user authorized broad reuse of recvx-enhanced-port and subsequently directed
us to prioritize a playable original campaign without rebuilding its systems.
Technical decisions are delegated; implementation proceeds in this chat.

## Architecture decision

Keep the existing Windows x64 prototype and its working changes intact. Add a
separately configured Windows x86 campaign target based on the MIT engine at
`aikatore/recvx-enhanced-port`, commit
`38d457e3ec11a3120447e3544031af9a40eccbdb` (0.6.8), and its modified decompilation
at `4aac5133901da3821fe443b79d6cdd1e3b29e6da`.
The original game logic, rendering boundary, movies, audio, saves, controls,
third-person camera and co-op are reused together. Do not paraphrase source to
hide its origin. Preserve copyright, licenses and source provenance.

The campaign is a native x86 executable, not an x64 port or an emulated PS2
executable. This is a deliberate change from the older x64-only implementation
direction to meet the user's current priority. Do not pretend the campaign
acceptance gate has passed simply because the reference was playable.

## Presentation and data

Build `AshfordsHalberd.exe` as the campaign executable in its own build and
package directory. Give F1 an original, resizable dossier-style settings panel:
left navigation, one selected category, search, contextual help, brass/ink
palette, and a compact status footer. Retain all underlying settings and input
binding functionality. Keep F2 tools and F3 networking functional.

Copy no reference icons, textures, screenshots, room/demo graphics or other
artwork. Draw the new panel with code; retail campaign graphics come exclusively
from the user's ISO. Optional user-supplied texture packs remain supported.
Ship no game data, proprietary headers or compiler binaries.

Use a separate SDL per-user `AshfordsHalberd/Campaign` writable directory,
keeping the prototype's configuration and incompatible saves untouched. Use
the engine's settings and save formats inside that directory, retain explicit
`--save-dir` support, and support ISO picker cancellation without a crash.

## Dependencies and acceptance

Use a local x86 GCC toolchain; do not change the global compiler. Pin engine and
decomp sources, and retain the reference's dependency versions. The user later
explicitly authorized fetching the missing Katana/CRI header dependencies;
initialize only those two submodules at the revisions pinned by the decomp.
Exclude all SDK headers/libraries from packages. Never silently substitute
guessed layouts. Permit independent platform tests without
these headers, with clear reporting that these do not validate the campaign.

Acceptance requires a built executable, startup with the user's validated USA
ISO, original New Game progression, controls, room traversal, inventory/combat,
sound/movies, save/load and in-game F1 verification. Full campaign completion
requires a real playthrough; record what is and is not verified. An unavailable
SDK header dependency is a build blocker, not successful integration.
