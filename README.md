# Ashford's Halberd

A native PC port project for Resident Evil: Code Veronica X, based on
[recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).
Players supply their own USA PS2 disc image (SLUS-20184).

This repository contains the first Windows application foundation: a native
executable, in-game main menu, ISO validation/import, and persistent settings.
It does not yet load playable rooms or run the original game logic.

## Run the Windows foundation

Build with `./tools/build.ps1` after bootstrap, or run the already-built
`build/windows/dist/AshfordsHalberd.exe`. Double-clicking the executable opens
the Windows ISO picker when no validated asset cache exists. Choose your own
CVX USA ISO; the game checks its boot target, executable hash, and 51-file
inventory, then imports its assets (about 1.8 GB) locally. After importing,
the original ISO may be moved without preventing startup.

Settings are in `%LOCALAPPDATA%\AshfordsHalberd\AshfordsHalberd.ini`.
Imported assets are in that directory's `assets` subfolder. Options in the
main menu currently control resolution, window mode, and VSync; Game Data
allows ISO reselection. Use the mouse, arrows/Enter/Escape, or a controller's
D-pad/A/B. Settings changes are saved immediately with backup/recovery handling.

New Game is marked in development. The original playable room slice and
Step 2's modern over-the-shoulder camera are not implemented in this build.
See [verification and limitations](docs/windows-foundation-status.md).

The revised Windows scope and first playable milestone are described in
[the Windows port design](docs/superpowers/specs/2026-10-02-windows-port-design.md),
including the optional modern over-the-shoulder camera, in-game settings,
first-launch ISO selection, and INI persistence.

## Windows development setup

Use Python 3.12, Git, a 64-bit MinGW GCC/G++ toolchain, and LLVM tools on PATH.
7-Zip is available for manual disc extraction. On this workstation these tools
are already installed; bootstrap records their paths in an ignored local file.
For another workstation, Scoop packages `git`, `python`, `gcc`, `llvm`, `gdb`, and
`7zip` provide these prerequisites. No global installations are performed by
the project scripts.

```powershell
./tools/bootstrap.ps1
./tools/verify.ps1
./.venv/Scripts/python.exe tools/inspect_iso.py 'Resident Evil - Code - Veronica X (USA).iso'
```

Bootstrap installs locked Python dependencies, including project-local CMake
and Ninja, and checks out the two pinned source dependencies. Verification
compiles SDL from source with GCC, links a native executable, and runs it.
The smoke executable checks timer/event initialization without requiring a
window. Build output is in `build/toolchain-smoke`.

The ISO inspector compares the executable SHA-1 with upstream's expected hash,
checks its ELF architecture, and writes an inventory, executable, and disc boot
configuration under `local/disc`. It does not upload the image or its contents.
ISOs, local game files, build output, and the Python environment are Git-ignored.
On this workstation's Python 3.12.4, pycdlib can emit an ignored `tp_clear`
`SystemError` during interpreter shutdown after a successful ISO inspection.
It is reproducible with just opening and closing a disc, including pycdlib
1.20.0 and 1.21.0. The inspection completes with exit code zero and validates
the executable; the shutdown issue remains unresolved.
To extract the complete disc for local research:

```powershell
7z x 'Resident Evil - Code - Veronica X (USA).iso' '-olocal/assets'
```

## Included tooling

| Tool | Purpose |
| --- | --- |
| CMake / Ninja | Native build configuration and compilation |
| GCC / G++ | Verified Windows x64 native compiler toolchain |
| Clang / clang-format | Additional compiler and source formatting |
| LLVM objdump / readobj | Binary inspection |
| GDB | Native debugging (the installed LLDB requires a missing Python 3.11 DLL) |
| SDL 2.32.10 source | Platform library candidate for window, input, audio, and timing |
| pycdlib | Read user-supplied ISO files |
| pyelftools / Capstone | ELF inspection and disassembly |
| splat64 / spimdisasm / Rabbitizer | Upstream-compatible MIPS analysis |
| Pillow | Texture/image inspection and conversion |
| pytest | Python tooling tests |
| 7-Zip | Manual disc extraction |

SDL is a starting dependency, not a finished renderer or an irrevocable choice
of port architecture. Movie, ADX audio, texture, and archive processing will
need format investigation before selecting additional runtime libraries.

## Source dependencies and boundaries

`upstream/recvx-decomp` is a Git submodule pinned to
`12a05ddfeef521d1f5e9a4082a182e788329135f`; its MIT license remains with it.
`external/SDL` is pinned to its 2.32.10 release and retains its zlib license.
Python versions are recorded in `tools/requirements.lock.txt`.

The reference project builds a PS2 MIPS executable with console SDK dependencies.
Its nested CRI, Katana, and CodeWarrior submodules are intentionally not initialized
by bootstrap. Sony SDK files are not included. Building the reference PS2 target
is a separate setup and is not required by the native toolchain smoke check.

The next porting work is to inventory platform calls, console data layouts,
graphics command paths, and asset formats; then design their native replacements.
A completed decompilation still requires this adaptation to execute on a PC.
