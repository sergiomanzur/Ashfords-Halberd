# Windows foundation — 2026-10-02

The first Step 1 increment is implemented. The full Step 1 playable slice
remains incomplete. A native room viewer is now available, with no adapted gameplay yet.

## Delivered behavior

- Native Windows GUI executable with SDL linked statically; no console,
  Python runtime, emulator, Sony SDK, or development tool installation required.
- First-launch native ISO picker, clean cancellation, background validation
  and asset import with visible progress, and rejection/reselection on errors.
- USA boot directive, executable SHA-1, and exact 51-file names/sizes checked.
  This does not assert a whole-disc preservation checksum or hash every archive.
- Import staging, previous-cache preservation, and recovery of interrupted swaps.
- Main menu with display resolution, windowed/borderless/fullscreen, VSync,
  and Game Data ISO selection. Keyboard, mouse, and controller menu navigation.
- UTF-8 INI settings under the Windows writable user-data folder, atomic
  replacement, valid backups, damaged-file preservation, and default recovery.
- Reuse of complete local assets even if the stored ISO path no longer exists.
- Per-user instance protection to avoid concurrent settings/import writers.

## Verification

`tools/build.ps1` compiles the executable, runs eight CTest entries, and packages
the result. The checks cover core settings/disc boundaries, native SDL/text menu
rendering, and standalone executable startup from another directory with only
the Windows system directory on PATH. Malformed test inputs include wrong
region/hash/boot target, unsafe filenames, extent overflow, directory cycles,
and truncation. Settings tests cover Unicode paths, missing/invalid settings,
backups, and preserving the valid backup after corruption.

All subsequent checks run headlessly, with SDL dummy video and software
rendering. They do not open a desktop window or exercise a visible file picker.
The additional asset suite covers archive bounds/overlaps, Expand literals,
short/long/extended back references, descriptor rollover, truncation, output
limits, and room/model offsets.

The real user-provided USA disc was inspected and imported with the native
executable. Integration tests verified inventory, rejected replacement without
changing working settings, cache reuse after setting a missing ISO path,
truncated cached files, missing manifest entries, incomplete discs containing
the correct executable, and recovery after moving the live cache aside.

Desktop verification exercised the native picker and cancellation, successful
GUI import, mouse navigation to Options, resolution change to 1280x720, VSync
change, immediate INI writes, normal Quit, relaunch without a picker, restored
window size, and rejection of a second process while the GUI was running.
The observed menu screenshot is in the ignored `local/windows-menu.png`.

The PE import table contains only Windows system DLLs. Verification was on this
workstation with an isolated executable search path, not on a separate clean VM.
Controller hardware, other monitors, exclusive fullscreen/borderless, and all
remaining resolution presets still need physical-device verification.

## Artifacts

- `build/windows/dist/AshfordsHalberd.exe`
- `build/windows/AshfordsHalberd-Windows-foundation.zip`
- `src/`: native application/core/text and USA inventory metadata.
- `tests/`: synthetic ISO fixtures, native boundary checks, runtime integration.

The GUI validation imported this user's ISO into the normal LOCALAPPDATA folder.
The local installation is therefore already configured; other users still
receive the first-launch picker. No ISO, extracted asset, or save is included
in the distribution or tracked by Git.

## Next Step 1 increment

The portable native asset module now reads AFS indexes, decompresses the game's
Expand streams, and validates room camera/collision/spawn tables and main model
object tables. All 205 entries in the user's USA RDX_LNK.AFS were decoded and
inspected successfully. This is metadata loading, not rendered gameplay.

Run the developer-only headless inspector after a build:

```powershell
./build/windows/AshfordRoomInspect.exe 'path/to/assets/RDX_LNK.AFS' 'local/rooms.tsv'
```

The report contains archive indices, byte sizes, versions and record counts.
It does not extract original assets into the distribution. The first room
expands to 5,392,084 bytes and contains three camera cuts, two enemy entries,
30 wall records and 67 main-model objects.

Native model/texture decoding and a headless overview renderer are now implemented;
see [native scene status](native-scene-status.md) for verified coverage and limits.

The native menu now includes Room Viewer with original camera snapshots;
see [viewer status](authored-camera-viewer-status.md).

Next, adapt player animation and movement, identify two connected rooms,
implement a native rendering backend, and begin adapting the original game
logic through explicit console-service replacements. The over-the-shoulder
camera remains a required Step 2 enhancement, with no nonfunctional toggle
presented in this foundation build.
