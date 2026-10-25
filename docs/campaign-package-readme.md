# Ashford's Halberd

Run `AshfordsHalberd.exe`, choose your unmodified USA PS2 Code: Veronica X ISO (SLUS-20184), then select Play Campaign. Keep the included assets and licenses folders beside the executable. Windows 10+ and OpenGL 3.3 are required. No game data is supplied.

This is an offline single-player build. Online sessions, co-op, chat, trading and server functionality have been removed.

The original launcher artwork/icon and ambient score were created for Ashford's Halberd. Launcher controls support mouse, Tab/arrows and Enter. Music can be muted or adjusted; display mode is applied when the campaign starts.

F1 opens the Field Dossier. The game's OPTION > ENHANCED OPTIONS menu uses the same video, audio, gameplay, camera, appearance and binding preferences. Both contain Cheats; F2 is a shortcut. F5 toggles third person and F11 toggles fullscreen.

Fixed camera: arrows move, W aims, Z interacts/confirms/fires while aiming, X runs/cancels, Enter opens inventory. Third person: WASD, mouse look, right mouse aim, left mouse fire, Shift run, E interact, Q cancel, R reload, Tab inventory. Bindings are editable.

Experience preset is at the top of Display and Gameplay in both enhancement menus. Original chooses 4:3, native internal resolution, original frame blending and classic movement; Enhanced chooses widescreen, automatic internal resolution, smooth frames, modern movement, skippable doors, faster loading, quick knife and automatic stairs. Custom keeps current values. Individual options stay available. Presets preserve bindings, saves, audio, camera tuning, renderer choices and cheats. Existing preferences are kept as Custom.

Saturation: 0% grayscale, 100% neutral, 200% stronger colour. Reset picture calibration also restores saturation. Optional FXAA edge smoothing is off by default and separate from internal resolution. It may soften game text/detail; the F1 overlay is drawn after filtering.

Cheats include health/ammo, healing, item grants, key locks, noclip, teleportation, spawning, title unlocks and rank tools. Story item grants and teleportation can disrupt event progression; keep a separate save. Runtime cheat toggles last for the session. Effects recorded into a save remain there.

Settings and saves: normally %APPDATA%\AshfordsHalberd\Campaign. Back up this folder. --save-dir overrides it; --skip-launcher bypasses the launcher. Prototype saves are separate and incompatible.

Development status: the original opening, inventory, movement in both camera modes and settings persistence have been exercised, and every retail movie passed decoding. Full room snapshots now replace the experimental native-checkpoint quickload. Scripted opening-room and cross-room restoration have been exercised. A complete campaign playthrough and late-game snapshot coverage remain unverified.

F9 quicksaves; F10 quickloads. Controller defaults: hold left-stick click (L3/LS) and press Back/View/Select to save, or Start/Menu/Options to load. Rebind both actions in either enhancement menu. Release the combination between actions. Requests require normal player control: menus, doors, cutscenes, scripted interactions, incompatible animations, countdowns and active targetable enemies block them. Blocked requests expire.

The single quickslot captures room objects, local flags, event tasks, random state, graphics and logical audio state. Quickload resumes directly without replaying room entry. Native saves, the continue point, ink ribbons and save/retry counters remain separate. Quicksave files can be tens of megabytes and briefly pause gameplay while writing or reading. Audio output may have a brief edge when its device buffer is recreated.

Quickslots are tied to this exact executable build. Updating or modifying the executable makes older quickslots incompatible; mismatched files are rejected before restoring game state. Keep native typewriter saves for progress across updates. The file is quicksave.ahs in the campaign save directory. Snapshot files contain local runtime game data and are not included in packages.

Campaign first person: choose Camera > Camera mode > First person in either enhancement menu, or press F12. Controller shortcut: hold L3/LS and press R3/RS; rebind it in Controls. Modern controller controls use the left stick to move and right stick to look/aim; keyboard/mouse uses the same WASD and mouse/action bindings as third person. FOV, sensitivity and dead-zone settings are shared. Scripted scenes, doors, puzzles, stairs/climbs/pushing, damage/grabs and native scopes temporarily retain their original cameras. The local body is hidden only while drawing first person. This camera mode uses existing weapon mechanics without a separate arm/weapon model set. Broad campaign playtesting remains necessary. --first-person selects it on startup.

The Alfred stairway script explicitly hides both actors before its sound sequence, so this build makes no speculative actor/visibility change; the exact encounter still needs save-based playtesting.

Engine adapted from aikatore/recvx-enhanced-port 0.6.8, Copyright (c) 2026 aikatore and contributors, MIT. Decompilation by AshfordFamily and contributors. Original source attribution and dependency licenses are in licenses/. Artwork/music provenance is in assets/branding/. This unofficial project is not affiliated with Capcom, Sony, Sega or CRI Middleware.

F8 skips the current FMV cutscene using the normal fade/cleanup. Rebind Skip FMV cutscene in either enhancement menu's control bindings. Real-time scripted scenes are separate.

Weapon quick switching: [ and ] cycle backward/forward; F3/F4/F6/F7 select the first four carried weapons in inventory-grid order. Controller defaults: hold left-stick click (L3/LS) and use D-pad Up/Right/Down/Left for carried weapons 1/2/3/4; hold L3/LS and press LB/RB (PlayStation L1/R1) for previous/next weapon. L3 alone does not switch. The combination consumes its other button until released, preventing an extra quick knife, reload or quick turn. Existing default-only controller settings upgrade automatically; custom assignments remain editable. All six actions are rebindable in F1 and OPTION > ENHANCED OPTIONS. Inventory changes update the order. Two-slot weapons count once; empty magazines are selectable. Release aim first. Scripts, menus, attacks, reloads, quick knife and incompatible movement/interaction animations block switching; blocked presses are discarded. No stored or unowned weapons are equipped, and ammo is preserved.


Reliability update (2026-10-24): corrected expanded-slot reload lookup,
puzzle/interaction input ownership, pushing while run is held, unavailable
vibration-row hover behavior and presentation scheduling edge cases. Includes
42,720 reload regression cases. The opening was exercised in both camera modes.
Exact late-game scenes and physical controller feel still need playtesting.
