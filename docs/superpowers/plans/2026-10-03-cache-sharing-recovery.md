# Windows asset-cache sharing recovery implementation plan

> Execute inline with superpowers:executing-plans/test-driven-development.

**Goal:** Tolerate short Windows sharing locks during asset-directory recovery
and import swaps; preserve cache data and fail within a bounded deadline for
persistent locks. This fixes a reproducible foundation dependency for Step1.
**Architecture:** One private directory-rename helper, no destination replacement,
up to10retries at50ms. Use it for recovery, live->previous, pending->live, and
failed-swap rollback. Existing cache validation/import ordering remains intact.
**Tech stack:** Windows x64 C++17/Win32 filesystem, existing CMake tests.
**Spec:** docs/superpowers/specs/2026-10-02-windows-port-design.md.
**Base:**e5be316; checkout clean. Native NG01 increment verified57/57/205rooms.

## Constraints and review focus

- Headless only; unique ignored scratch roots; normal LOCALAPPDATA untouched.
- No process shutdown, sync-setting changes, permission changes or GUI tools.
- The original intermittent failure's responsible process is unverified;
  reproduce a controlled handle without FILE_SHARE_DELETE instead of blaming it.
- Retry only sharing/lock/access-denied results, at most500ms total requested
  delay; other errors fail promptly with source/destination/error diagnostics.
- Existing destination is not replaced. Persistent failure leaves previous
  cache available; successful recovery is an atomic directory rename.
- Source/target remain fixed children of the caller's asset root; no extra
  recursive moves/deletes or changes to ISO identity validation.
- Full gameplay/audio/inventory/save acceptance remains required for Step1.

## Task1: deterministic lock regression and bounded rename

Files: tests/core_tests.cpp,src/core.cpp.
Interface: existing recover_import(root), import_disc(disc,root,progress);
new helper stays private and retains rename failure behavior for permanent
or nonsharing errors.

- [x] Test first: open assets.previous directory without delete sharing,
  release handle after150ms, recover_import succeeds and retains marker.
  Persistent lock fails within400..5000ms preserving previous/no live; retry
  after release succeeds. Existing live takes priority and retains both files.
- [x] Build core-tests and run core-boundaries. Expected:FAIL on transient
  recovery while lock is held, with handles/threads safely released.
- [x] Implement private bounded Windows directory move and use at all four
  asset rename sites. Expected:core-boundaries PASS with deterministic short
  and persistent lock behavior and all previous INI/malformed-ISO checks.

## Task2: final gates

Files: docs/windows-foundation-status.md,this plan.

- [x] One fresh read-only review of the complete increment; material issues
  one RED/GREEN fix pass plus full suite; Minor gaps recorded/deferred.
- [x] tools/build.ps1. Expected:57/57checks and refreshed executable/ZIP.
  Final205-room scan and fresh packaged dummy-SDL smoke: all current grab/
  corridor probes retained;241ticks/240renders/53moves/10blocked/menu/INI.
- [x] Headless native real ISO import and reimport into a fresh ignored profile;
  native cache checks and ISO INI persistence pass; normal user INI unchanged.
- [x] Format/diff gates, evidence/rulings ledger, docs, commit; keep full Step1
  goal active until complete gameplay/audio/inventory/save acceptance.

Verified:57/57checks;205rooms;436NG01updates/852paired samples/10recoveries;
fresh packaged smoke241ticks/240renders/53moves/10blocked/menu/INI creation.
Real ISO initial import and replacement both pass. No forced sharing lock was
introduced during promotion or rollback. Review found no material defects;
Minor exception type/path/error-code assertions are deferred.
