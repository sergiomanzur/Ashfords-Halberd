# Ashford's Halberd: Windows scope and Step 1 design

Status: scope revised from the user's instructions and authorized for
implementation. The Windows foundation and native scene decoding checkpoints
are implemented; the Step 1 playable slice remains in progress.

## Product intent

Run Code Veronica X as native Windows x64 code derived from the pinned USA
decompilation. Players provide their own SLUS-20184 ISO. Future Android,
Linux, SteamOS/AppImage, and macOS releases should reuse the game core.
The initial development platform is Windows; support on other systems is
not part of the first release's acceptance criteria.

## Step 1: native playable slice

The milestone is a real slice of the original game: load a room from retail
assets, display its geometry and animated player, reproduce the original
camera behavior, accept movement and interactions, transition to another
room, and demonstrate sound, inventory, combat, and save/load. A launcher,
asset viewer, or blank SDL window alone does not complete this milestone.

Work is divided into independently verifiable increments:

1. Native executable foundation: first-launch ISO selection, disc validation,
   INI persistence, platform window/input, and an in-game main menu with
   implemented settings. This is the first implementation increment.
2. Asset access and rendering: inventory required archives, parse one actual
   room and its models/textures, and render it with the original camera.
3. Gameplay slice: port the required player, event, collision, room transition,
   and interaction code from upstream into the native core.
4. Slice completion: inventory, combat, decoded audio, and a versioned save/load
   round trip across two rooms.

The exact pair of rooms will be selected from decoded retail room data after
asset inspection; selection must include an enemy, an interaction, and a
traversable connection. Room selection is an investigation deliverable, not
an assumption that the opening sequence already meets all criteria.

## Revised Step 2: Windows 1.0

Include the full campaign, original puzzles/progression and unlockables,
original timing, reliable saves, and compatibility playthroughs.

Presentation includes configurable resolution, windowed/borderless/fullscreen,
original 4:3 framing, optional validated widescreen, scaled text/UI, aspect-correct
movies, and an original presentation preset. Controls include remappable
keyboard/controller bindings, hot-plugging, vibration, and matching prompts.
Include separate music/voice/effects volume, focus-loss pausing, settings
persistence, save backups, and safe door/cutscene skipping.

### Modern over-the-shoulder camera

An optional RE2 Remake-style mode is required in Step 2, selectable alongside
the original camera in the game's main menu. It includes third-person follow
and orbit, mouse/right-stick look, shoulder offset, camera-relative movement,
over-the-shoulder aiming with a reticle, adjustable sensitivity/inversion,
and obstruction handling that prevents the camera entering walls.

Camera selection must preserve progression and existing saves. Scripted
sequences and cutscenes retain their authored camera and restore the chosen
gameplay camera afterward. Original mode retains its original control and
targeting behavior. Modern aiming requires a defined hit-testing and weapon
alignment implementation; merely rotating the view does not satisfy it.

Free viewing requires auditing room geometry, camera-dependent visibility,
effects, clipping, and areas not normally exposed. Room-specific fixes must
be tracked and tested. This enhancement enlarges Step 2; it is not silently
deferred to a later optional release. It is not required to finish Step 1.

### In-game settings

All player-facing options and enhancements belong under the main menu's
Options entry, grouped as Display, Graphics, Audio, Controls, Camera,
Gameplay/Enhancements, and Game Data. Reuse the same settings UI from the
pause menu where gameplay allows it. Main-menu access remains mandatory.
Do not add a separate configuration launcher. Only implemented features are
offered as working controls; planned enhancements must not appear functional.
Expose Change ISO under Game Data and persist applied choices immediately.

## Executable startup and ISO ownership

Double-clicking `AshfordsHalberd.exe` is the normal launch flow; the shipped
runtime must not require Python, developer scripts, or a console.

1. Locate the user-data directory and read `AshfordsHalberd.ini`.
2. If no usable game data is configured, open a native Windows ISO file picker.
3. Validate ISO structure, USA boot identity, and the extracted executable
   against upstream SHA-1 `f6c26ed664c2de72627fea5323b0d6a931601e13`.
4. Reject incompatible or damaged input with an actionable message and allow
   another selection. Cancellation exits cleanly without committing a bad path.
5. Import required assets into a local cache using a temporary staging area;
   commit its manifest and ISO setting only after successful validation/import.
6. Start the in-game main menu. Subsequent launches use the validated cache.

The stored ISO path is provenance and enables reimport. If a complete validated
cache exists, a moved/disconnected ISO does not force selection again. If data
is incomplete, allow retry or ISO reselection. Changing ISO cannot destroy a
previous working cache until replacement succeeds. The game executable is
used for validation and research; it is not emulated or executed at runtime.

## Settings and writable storage

Default Windows location: `%LOCALAPPDATA%\AshfordsHalberd\AshfordsHalberd.ini`.
Use the OS-specific writable user-data directory through a platform interface
on future systems. Do not depend on the process working directory or writable
installation folders. Store imported assets and saves beneath the same root
in separate directories. Exclude all local data from Git and distributions.

One INI persists all user settings: schema version, ISO/cache selection,
display/graphics/audio values, input bindings, camera mode and sensitivities,
and enhancement toggles. Save-game progress remains in separate versioned
save files; it is not configuration. Example sections are `[General]`,
`[GameData]`, `[Display]`, `[Audio]`, `[Controls]`, `[Camera]`, and `[Enhancements]`.

Parse UTF-8 paths, validate values, fill absent keys with defaults, and reject
invalid individual values without discarding valid settings. Write atomically
through a temporary file and preserve a last-known-good backup. Surface write
failures in the game UI. Handle damaged configuration with recoverable defaults
and preserve the damaged file for diagnosis. Avoid conflicting simultaneous
instances through a Windows per-user instance guard.

## Technical direction and alternatives

Recommend C++17 for the application/platform adapters and C for adapted game
logic, CMake/Ninja for builds, and the existing SDL dependency for windowing,
input, timing, and audio output. A renderer interface isolates the eventual
graphics backend; compare portable GPU approaches after decoding the initial
room rather than selecting one without evidence.

A Windows-only rewrite could simplify the first platform but would duplicate
work on future ports. An emulator-backed wrapper would start faster but fails
the native-code requirement. The recommended approach retains the decompiled
game logic and replaces console services with native, testable interfaces.

Keep the pinned upstream checkout unmodified as reference. Place adapted game
sources in a distinct native core, preserve attribution, and record their
upstream origin. Do not download proprietary SDKs to satisfy missing headers;
inventory required declarations and implement replacement interfaces based
on the code and required behavior.

Proposed boundaries: application/menu, settings, Windows platform, disc/import,
archive/asset readers, game core, renderer, audio, and saves. The Windows file
dialog is the only initial platform-specific user flow. Game logic receives
input states, elapsed simulation ticks, asset access, and platform services;
it does not call Win32 directly.

Treat console offsets, pointer widths, alignment, and endian rules explicitly.
Decode on-disc layouts into runtime structures rather than mapping PS2
structures containing 32-bit pointers onto x64 objects. Never satisfy a required
gameplay service with a silent no-op just to obtain a successful link.
Determine the original update cadence from source/data and preserve it using
fixed simulation ticks separate from presentation refresh.

## Findings from the current source inspection

`main.c` owns shared game state and the task dispatch table, includes PS2 and
Katana headers, and performs fixed-size memory initialization/arithmetic.
`game.c::bhMainSequence` couples player/enemy/event updates with model drawing.
`camera.c` uses Ninja matrix operations and clears a fixed-size camera block.
`ps2_dvd_image.c` enumerates retail RDX room resources. These are useful porting
seams, but the PS2 sources cannot simply be added to the x64 smoke target.
The SDK dependencies and generated/missing declarations need an inventory.

## Verification and acceptance

For the first increment: test absent/corrupt INI, Unicode/space-containing paths,
picker cancellation, incorrect region, truncated ISO, invalid executable hash,
cache interruption, successful relaunch without a picker, and settings surviving
a process restart. Verify double-click-equivalent launch from a different working
directory and a distribution containing only its runtime dependencies.

For Step 1 completion: render actual imported game assets; verify movement,
camera transitions, interactions, an enemy encounter, inventory, sound, and save
reload across the selected rooms. Compare behavior against original-game reference
observations. A build with placeholder geometry or disconnected synthetic gameplay
does not satisfy the acceptance criteria.

For Step 2: full playthroughs in original and modern camera modes, camera collision
and visibility audits, controller/keyboard testing, refresh-rate testing, and
save compatibility. Higher-frame-rate rendering, checkpoints, replacement packs,
randomizers, and broader mod APIs remain separately scoped future work.

## Review boundary

The user's instruction authorizes beginning Step 1. The brainstorming workflow
requires review of this written technical design before writing the implementation
plan, followed by review of that plan and selection of its execution method.
No product implementation has been performed at this boundary.
