# Windows settings sharing retry implementation plan

> Use superpowers:executing-plans inline for Step 1 INI persistence.

**Goal:** Persist settings when a brief Windows sharing lock blocks replacement.
**Architecture:** Keep the existing backup/temporary-file transaction. Retry only
ERROR_SHARING_VIOLATION, ERROR_LOCK_VIOLATION and the ambiguous
ERROR_ACCESS_DENIED response, up to ten 50 ms delays.
**Tech stack:** Existing C++17 Win32 settings core and headless core-tests.
**Spec:** ../specs/2026-10-02-windows-port-design.md

Constraint: normal profile untouched; tests use local/core-tests. Do not bypass
permanent permissions errors or delete a user's original settings.

## Task 1: Bounded sharing-lock retry

Files: src/core.cpp, tests/core_tests.cpp.

- [x] Open the diagnostic INI without FILE_SHARE_DELETE, release its handle
      after 150 ms, call save_settings, and observe the existing save failure.
- [x] Retry atomic MoveFileExW only on Windows sharing/lock violations, at
      most ten retries with 50 ms delay. Include ERROR_ACCESS_DENIED because
      headless GDB confirmed this response for the controlled sharing lock.
      Other failures retain existing error.
- [x] Verify updated INI data and the original backup after eventual save;
      test a persistent lock still fails within the bound and leaves the
      previous settings intact.
- [x] Run full CTests, package build, retail probe and packaged headless smoke;
      commit with the final evidence. The observed initial full-suite INI
      failure was transient; the controlled lock test proves this correction.

Ruling: the controlled FILE_SHARE_DELETE-denial fixture returned Windows error
5 at the MoveFileExW failure, proven with headless GDB. Retry that ambiguous
access-denied response as well; a permanent access-denied failure may therefore
take up to 500 ms to report. The original INI remains intact. Fresh per-run
diagnostic profiles prevent a RED run's partial settings from affecting reruns.

Verification: original controlled lock failed, then retry of sharing/lock-only
responses also failed. Headless GDB proved actual error5. Including the ambiguous
access-denied response made short-lock recovery GREEN. Persistent lock test
verifies an actual retry grace period, bounded failure and intact original INI.
The read-only review found no product defect. Fresh per-run diagnostic profiles
resolve partial RED-run state; a minimum persistent-lock duration strengthens
coverage alongside the timed release. Final build passed all22 CTests and
packaged headless runtime smoke (241ticks/240frames). Default profile untouched.
