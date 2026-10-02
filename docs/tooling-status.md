# Tooling baseline — 2026-10-02

Target for the initial setup: Windows x64 native code. Game logic has not yet
been adapted or compiled for the PC.

- Reference: recvx-decomp `12a05ddfeef521d1f5e9a4082a182e788329135f`.
- Platform source dependency: SDL 2.32.10,
  `5d249570393f7a37e037abf22cd6012a4cc56a71`.
- Native compiler: GCC/G++ 15.2.0, `x86_64-w64-mingw32`.
- Additional compiler/inspection tools: LLVM 22.1.8.
- Debugger: GDB 17.1, installed through the existing user-level Scoop setup.
- Python: 3.12.4, isolated in `.venv` with a complete pip version lock.
- CMake 4.4.3 and Ninja 1.13.2 are installed in that environment.
- 7-Zip 26.02 is available for manual disc extraction.

The local ISO has 51 files and a 32-bit MIPS `SLUS_201.84` executable.
Executable SHA-1 `f6c26ed664c2de72627fea5323b0d6a931601e13` matches the
USA configuration in the pinned upstream checkout. This verifies the
executable against upstream, not a whole-disc preservation database checksum.

No Sony SDK, CRI SDK, Katana SDK, or CodeWarrior nested checkout was fetched.
Those belong to the original PS2 build path and are not dependencies of the
native smoke check. Game assets stay in ignored local directories.

Known limitation: pycdlib emits a CPython shutdown cleanup error after ISO
operations on this Python installation. Validation finishes and produces the
local inventory with exit code zero. The issue is documented in the README.
The installed LLDB fails due to a missing Python 3.11 DLL; GDB is the selected
debugger for this setup.

Verification completed:

- `tools/bootstrap.ps1` succeeded with the locked dependencies.
- `tools/verify.ps1` built SDL and a native executable; CTest passed 1/1.
- The smoke executable reported SDL 2.32.10 and 64-bit pointers.
- GDB launched the smoke executable, which exited normally. This GDB build
  warns that XML library-list support is disabled.
- Python tool imports and `pip check` passed.
- ISO inspection matched the expected USA executable and listed 51 files.
- Git ignore checks confirmed the ISO, extracted executable, inventory,
  environment, and build outputs are excluded.
