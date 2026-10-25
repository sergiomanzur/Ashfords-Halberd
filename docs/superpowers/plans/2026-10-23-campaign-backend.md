# Campaign Backend Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task. The user requested autonomous reuse; execute inline.

**Goal:** Bring the reference campaign runtime and QoL into Ashford's Halberd with original presentation.

**Architecture:** A separately configured x86 campaign target preserves the x64 prototype. Reuse the MIT engine and its exact decomp fork together.

**Tech Stack:** C/C++, GCC x86, SDL3, OpenGL 3.3, ImGui, CMake/Ninja, PowerShell.

**Spec:** `docs/superpowers/specs/2026-10-23-campaign-backend-design.md`

## Global Constraints

- Preserve existing working changes and prototype tests.
- Pin engine 38d457e3ec11a3120447e3544031af9a40eccbdb and decomp 4aac5133901da3821fe443b79d6cdd1e3b29e6da.
- No copied graphics or game data; exclude SDK dependencies from packages and preserve MIT notices.
- Keep campaign saves separate from prototype saves.

## Review Focus

- Windows paths containing spaces and apostrophes: configure/build must work.
- Missing headers/compiler: fail with actionable local paths; fetch only the two pinned header trees with explicit -FetchHeaders.
- Missing ISO or cancelled picker: exit cleanly without corrupting configuration.
- Small windows and long settings lists: navigable panel without clipping.
- Gamepad/mouse capture and settings writes: retain reference behavior.

### Task 1: Source integration and separate build

- [x] Add a source boundary test that rejects bundled graphics and checks pinned sources and packaged attribution; run it red.
- [x] Import the engine's source/tools/tests, excluding artwork and demo mods, with provenance.
- [x] Register the pinned decomp fork; after explicit user authorization, initialize only the Katana/CRI dependencies, leaving the original compiler uninitialized.
- [x] Add separate campaign configure/build and platform-test paths; verify missing dependencies are diagnosed before configuration.
- [x] Build the independently testable runtime components with local x86 GCC.

### Task 2: Original F1 panel and writable storage

- [x] Replace F1 window composition with a sidebar, category search, contextual help and status footer, retaining the shared settings table and binding editor.
- [x] Add original ImGui style, keyboard navigation and scalable sizing; copy no artwork.
- [x] Set campaign executable/window names and local writable storage; preserve command-line paths and engine save format.
- [x] Compile changed platform translation units and exercise UI layout with an ImGui test harness when full game headers are unavailable.

### Task 3: Verification and packaging

- [x] Run engine platform tests and source boundary tests.
- [x] Build/start the campaign if required headers exist; otherwise report the exact blocker.
- [x] Package only a successful campaign build with dependency license notices.
- [x] Record verification and remaining campaign playthrough requirements.

## Evidence and limits

- Full optimized Windows x86 GUI executable linked successfully.
- 19/19 CTest checks passed with the real USA ISO; no skips (154.31 s).
- Source boundaries: 4/4 passed; native diagnostic-tool regression checks: 2/2 passed.
- Corrected the reference binding test's uninitialized event-union storage;
  20 consecutive optimized binding runs passed.
- Live F1 screenshot and a mouse-driven VSync change verified presentation,
  application and persisted settings. UI layout checks cover small/resized windows.
- Original opening room, actors, music, dialogue and inventory executed.
  A 2400-frame fixed-camera run recorded 400 controlled movement frames and
  180 distinct player positions, without warps or injected progress.
- The installed executable passed the same opening in third person, with 304
  distinct player positions; its screenshot and native runtime logs were inspected.
- ZIP allow-list verified: one executable, README and ten notice/license files.
- Full campaign completion, natural cross-room save recovery and real online
  co-op sessions remain playthrough requirements, not claimed successes.
