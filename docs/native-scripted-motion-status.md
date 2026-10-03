# Native original scripted motion

The native common24 selector1 now executes the selected original room motion
sequence. Root and linked body use the room bank; the independent base bank
remains available. Speed uses the original26-entry table; low-byte motion
selection, blend6 and larger blend counters retain their source meanings.
Immediate actions0..7 sample with explicit add0/mode0 and calculate the exact
self/link combinations. The body is not implicitly sampled by this command.

Model calculations retain per-work base matrices and shared pose-owner caches.
CalcLinkModel visits active linked enemies, objects, items and effects in source
order, including hidden actors. Each child sees prior shared-cache writes.
Linked effect matrices are stored in a replacement immutable pool. Malformed
links reject before publishing the calculation. Entry work references remain
stable; a non-sampling selection validates its motion record when consumed.

Original subpl event callbacks0..14 now run with their source static, sampling,
turning and movement behavior. Zombie mode5 runs the event callback before the
mandatory MainLoop motion phase, preserving source single/double sampling.
Body mode6 bypasses the event table and still samples upper-body channels.
Generic subpl modes5/6 calculate their models and complete damage cleanup.
Out-of-range callback indices and malformed sampled records reject explicitly.
Other selectors and unsupported controllers remain required boundaries.

The unchanged production entry probe passes common24 in retail RDX2, reaching
original opcode11 at364 (bhEtcAtariSet). RDX3 still requires opcode139 at276,
computer event0. Corridor RDX1 enters and advances120 ticks. No opcode skips
or exception suppression were added. This is original script progress; the
cemetery has not completed entry and retail enemy gameplay remains unverified.
Evidence: local/scripted-motion-entry-audit-verified.log.

The corrected full Windows build and package passed99/99 headless checks.
Regression tests verified source deferred-record selection and stable bound-work
references after the initial full-suite failure. Evidence:
local/scripted-motion-full-build-corrected.log. A fresh packaged dummy runtime completed241ticks/240renders and53movement
ticks, generated192800 audio frames, submitted184000 and returned with queue0.
The ZIP has the six executable/readme/license files; its EXE matches dist.
Evidence: local/scripted-motion-final-smoke.log. This checks corridor runtime
and output lifetime; it does not verify audible cemetery sound or full gameplay.
One fresh read-only review checked all five focus areas and found0Critical,
0Important and2Minor documentation issues. No corrective pass was required.
Deferred: the mtn_tp rationale must describe its destination/mirroring table,
not a cached record; mode100 terminology must describe direct blended position
assignment, not ping-pong. The selected source behavior is correct; full scope
and prior deferred work remain unchanged. Rulings and review details:
local/native-scripted-zombie-motion-progress.md.
Complete Step1 still
requires the two connected rooms, enemy gameplay, combat, inventory, complete
sound/effect presentation, versioned cross-room save/load and independent
original-game reference acceptance. New Game remains disabled.
