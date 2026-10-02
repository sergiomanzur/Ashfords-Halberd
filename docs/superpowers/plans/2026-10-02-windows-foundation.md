# Windows Foundation Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task. The user requested implementation in this session.

**Goal:** Deliver the first Step 1 increment: a distributable Windows executable with native ISO selection, validated asset import, persistent INI settings, and an in-game main menu.

**Architecture:** A C++17 core owns settings, ISO9660 validation/import, and cache validation. SDL handles the native window, renderer, and input; a Windows adapter supplies file selection, writable storage, hashing, and text rasterization. No original gameplay is claimed in this increment.

**Tech Stack:** C++17, SDL 2.32.10, Win32, BCrypt, CMake/Ninja, GCC x64.

**Spec:** ../specs/2026-10-02-windows-port-design.md

## Global constraints

- Windows x64 first; no emulation or Python in the shipped runtime.
- USA SLUS-20184 executable SHA-1 must match the pinned reference.
- Main-menu access for implemented settings; persist in AshfordsHalberd.ini.
- User data under LOCALAPPDATA; do not assume writable install folders.
- Cache replacement only after successful import; keep assets out of Git.
- Modern camera is Step 2; do not expose it as working now.

## Review focus

- Unicode paths and launch from a different current directory.
- Missing/corrupt INI and invalid values preserve valid settings.
- Malformed extents, unsafe ISO names, wrong region, and truncated input fail safely.
- Failed import/reselection preserves the previous working selection.
- Missing original ISO with a valid cache still permits launch.

## Files and responsibilities

- CMakeLists.txt: core, executable, tests, and runtime packaging.
- src/core.hpp / core.cpp: configuration, ISO9660, SHA-1 validation, transactional import.
- src/main.cpp: native startup, file picker, SDL menu, graphics settings, single-instance guard.
- tests/core_tests.cpp: INI and ISO/cache boundary tests.
- tests/fixtures.py: synthetic disc fixtures including malformed records.
- tools/build.ps1: compile, test, and collect the runnable distribution.

## Task 1: settings and disc services

- [x] Write tests for settings round-trip, malformed INI, wrong region/hash, truncated and unsafe disc records.
- [x] Compile the tests before production files exist and verify the missing API failure.
- [x] Implement `Settings load_settings(path)`, `save_settings(path, Settings)`, `Disc inspect_disc(path)`, `import_disc(Disc, root, callback)`, and `cache_valid(root)`.
- [x] Run tests and inspect a real user-supplied disc; compare its hash and inventory with the existing Python inspector.

## Task 2: native executable and menus

- [x] Add runtime integration checks for `--inspect-iso`, `--import-iso`, `--check-cache`, and `--smoke-test`.
- [x] Implement startup with writable user data, single-instance protection, native picker, validation/import progress, and persisted selection.
- [x] Implement the main menu and display/game-data options; disable New Game until the real playable slice exists.
- [x] Verify renderer startup, configuration persistence, cache reuse, and clean exit.

## Task 3: deliver and document

- [x] Build a self-contained Windows GUI executable with static GCC runtime and SDL.
- [x] Run CTest, native integration checks, and inspect the menu visually if desktop capture is available.
- [x] Package under build/windows/dist and document the executable location and current limitations.

## Execution ledger

Ruling: proceed inline under the user's explicit "please go ahead and implement" instruction; do not repeat design/plan approval requests.

Ruling: work in the existing workspace to preserve its uncommitted tooling and reuse installed dependencies; no checkout reset or unrelated edits.

Verified: the foundation build and native retail integration passed. Subsequent checks use SDL dummy video and software rendering under the user's headless-only instruction. The prior desktop checks occurred before that instruction.
