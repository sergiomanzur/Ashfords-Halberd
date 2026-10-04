# Native enemy defeat reconciliation

Implement inline under the authorized full Step1 goal, using the current
codex/windows-foundation checkout. BASE:1f9946496f7bac9d286f51689bfd5ac1d56ee7da.
The previous turn made progress: original lighting committed with140 passing
tests, a fresh packaged headless run and unchanged retail room audits.

Source: pinned event.c::bhDieCk. Instruction13 consumes4 bytes: opcode,
authored enemy index and a little-endian16-bit defeated-progress index.
Use the original ten-bit word selection and MSB-first bit in ed_flg/native
bank3. If already recorded, do not read the authored enemy/work mapping.
Otherwise resolve that existing mapping and record only when work.flags has2.
Do not mutate health, entity flags or any other bank. Preserve instruction34's
different original nine-bit mask.

This implements the actual persistent state consumer required by the cemetery
script, not a skipped instruction. It does not finish enemy AI or the full
playable slice. New Game remains gated; connected rooms, combat, inventory,
decoded sound/effects, versioned cross-room saves and original-game acceptance
are still required. Keep all tests headless with isolated ignored profiles;
no imported retail data in Git/distributions, no desktop windows or audio.

1. [x] Revalidate clean BASE and genuine cemetery13/6 boundary. Capture96 cases
   from the unchanged original function; observe reconciliation/reference RED
   against the current library. Validation-only rejection is not genuine RED.
2. [x] Promote tests/scenario_enemy_defeat_tests.cpp and source-derived
   tests/enemy_defeat_reference_fixture.hpp. Add checked instruction13 in
   src/scenario.cpp and3 CTest modes in CMakeLists.txt. Verify all48 behavioral
   combinations,96 reference cases, malformed extents/maps and recorded-map
   bypass. Format new files and only the changed existing code range.
3. [x] Run the full Windows build/tests/install/package (expected143 tests),
   fresh dummy SDL packaged smoke and exact ZIP/EXE check. Compile/run the
   unchanged retail entry probe and record the next genuine production boundary
   without script edits, forced flags or skipped original controllers.
4. [x] Record status/evidence; check staged paths and whitespace; commit this
   small source instruction increment locally. Leave the full goal active.

Existing user authorization covers the concrete source behavior and inline
implementation. No new approval handoff or fresh lighting re-review is needed.
This small instruction uses unchanged-source capture and author verification;
major subsequent enemy orchestration requires its broader review workflow.
