# Full room quicksaves

F9 saves a single quickslot; F10 restores it. Controller defaults are **hold L3/LS and press Back/View/Select** to save, or **hold L3/LS and press Start/Menu/Options** to load. Both enhancement menus expose rebindable actions. Release the combination before another action.

Quicksaves require normal player control. Menus, doors, cutscenes, scripted interactions, attacks, reloads, stairs, damage, quick knife, pushing, countdowns and active targetable enemies block requests. Blocked requests expire rather than executing later. Simultaneous save/load presses cancel.

The quickslot captures the engine arena, player and room objects, local flags, event tasks, random generator, emulated platform state, graphics resources and logical audio state. Loading resumes that state directly instead of reopening the room through the original retry path. It neither consumes ink ribbons nor increments the native save or retry counters. Current display, volume and input preferences remain in effect.

`quicksave.ahs` lives beside `recvx.ini` in the campaign save directory. Native typewriter saves and the continue point remain separate. Snapshot files may be much larger than native saves, and writing or reading briefly pauses gameplay. The container compresses zero-filled memory, checks all sections and replaces the previous slot only after a successful atomic write.

Snapshots are specific to the **exact executable build**. A SHA-256 build fingerprint rejects incompatible files before restoring game state. Keep native saves for progress across updates. Earlier experimental native-checkpoint quickslots are rejected. Snapshots contain local runtime game data; they are excluded from packages.

Graphics and audio use explicit resource restoration rather than copying host handles. Audio decoder, synth and reverb state continue, but the device's internal resampler/output tail is recreated and may produce a brief edge. Developer spawn/survey environments disable snapshots because those tools retain process-owned pointers.

The optional campaign first-person camera is now available separately. Full snapshots preserve its selected mode; see [controls and limitations](campaign-first-person.md).

## Verification on 2026-10-24

The full build completed. CTest passed 38 runnable tests; the movie test was skipped because this invocation had no disc environment configured. Quicksave guards and input behavior include 149 checks. Renderer, native texture, audio, RNG, cheat-room state, manifest and container regression tests passed.

Gameplay fixtures saved in the opening cell block, changed health and equipment, loaded another room, then restored the original state without reading its room file again. Exact position, health, inventory, room flags and native save/retry counters matched. Movement and another save succeeded afterward. This was exercised with fixed and third-person cameras; the final executable was tested in third person. A fresh process of the staged distribution then loaded a copied quickslot from a different save folder, restored the expected state and saved again after movement.

These fixtures use real controller bindings plus explicit test-only equipment, health and teleport commands. They establish targeted restoration behavior, not a complete campaign playthrough. Late-game scenes and physical controller feel still require playtesting. The tested quickslot was approximately 58 MiB; size depends on loaded resources.

Tested executable SHA-256: `769951c8a9bef4493bb437d1feed942cbcd190e8c26b28a779d4359d3f0bb888`.
