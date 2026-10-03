# Native original room entry

Original event.c bhEneSetCk (opcode34) now consumes its four-byte instruction,
reads the9-bit defeat flag with MSB-first bits, and hides the authored mapped
enemy only when that flag is set. Clear flags leave already hidden work alone
and do not resolve a missing enemy map. Required invalid work and truncated
instructions reject before changing entity flags.

bhEffModeSet (opcode145) now assigns the original mode1 through sys->efid's
native authored-slot map. Immutable pool replacement preserves earlier room
snapshots, other mode bytes and inactive/aliased works. Assignment does not
allocate a work slot; a full pool can still receive mode changes.

Common_controll cases23 and46 now operate on the existing bound work context.
Case23 clears/sets118 or18 for variants0..3, leaving unmatched variants alone
without requiring work. Case46 clears20 for word0 and sets it for any nonzero
16-bit word. They retain their four-byte cursor extent and preserve other
actor fields. Native enemies, objects and items use their actual bound work.

Literal tests reproduced the missing operations before implementation, including
the real room loader145/67 initialization sequence. Five focused checks passed.
An incorrect test assumption that END deactivated the task was corrected after
checking source bhEnd; the test now asserts its actual END cursor instead.

The production probe executes unchanged selected original room scripts, without
opcode skips. Corridor RDX1 enters and processes120 native ticks. RDX2 passes
its effect-mode and bound-work controls and now requires the common motion path
or companion. RDX3 reaches opcode139 at276, requesting source computer event0.
That request's original sp_evt.c consumer is unported, so it still rejects
explicitly rather than silently consuming a flag. Both graveyard entries remain
incomplete. Evidence: local/room-entry-activation-audit-verified.log, rebuilt
against the final production library after all source edits.

The full Windows build/package passed95/95 headless checks. A fresh packaged
runtime profile completed241 native ticks/240 rendered frames and53 movement
ticks, generated192800 audio frames, submitted184000 and left an empty queue
after returning to the menu. All output used dummy video/audio; corridor entry
requests no ambient sound, so this is an output/lifetime check. ZIP inspection
confirmed six runtime/license files and no retail assets. Logs:
local/room-entry-activation-final-build.log and
local/room-entry-activation-final-smoke.log.

One fresh read-only review inspected all five focus areas, source handlers and
retained evidence, finding no Critical, Important or Minor issues in this
increment. No corrective pass was required; previously deferred work is unchanged.

Complete Step1 still requires full original entry/gameplay in the connected
rooms, enemy AI, combat/inventory, complete audio/effect drawing, cross-room
versioned save/load and independent original-game comparison. New Game stays
disabled. This increment does not establish full-slice acceptance.

Subsequent original scripted motion work passes common24 and now reaches
opcode11at364 in RDX2. See docs/native-scripted-motion-status.md for current
boundaries; the earlier entry evidence above belongs to its own increment.
