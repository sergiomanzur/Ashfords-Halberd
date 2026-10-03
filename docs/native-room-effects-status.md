# Native room effects and shadows

The Windows port now decodes the original68-byte EF_WRK room table and binds its
authored effects to the same512-entry native pool used by enemy effects. Native
records preserve signed fields and source narrowing, validate finite values and
the450-model ID range, and reject malformed tables before replacing live state.
Source flags3 determine first-free allocation; inactive records can alias the
following birth, matching the original allocator. An exhausted authored binding
rejects atomically. Immutable pool copies preserve the previous room on failure.

The room loader binds effects before initializing enemy controllers. Opcode67
changes visibility through its authored effect-to-slot map. Opcode47 maps the
authored enemy work, honors the800 duplicate gate and allocates a real source
shadow with its variant dimensions. Pool exhaustion returns no shadow slot while
retaining the source duplicate gate. Zombie startup now allocates root/body
shadows in this shared pool and preserves their indices during reinitialization.

Source bhEff001 shadow control follows cached linked joints, initializes its
modifier mode on the first tick, then computes ground height, owner yaw, local
joint extents and base-to-double-base clamps. Visibility follows original pt_flg
and owner flags. Hidden/gated effects retain their state. A failing cached joint,
matrix or required ground branch rejects the whole staged pool. An inactive
owner releases its shadow. Native callbacks run after player control alongside
the existing wall timers; remaining original effect callbacks are unfinished.

At commit49c2f59 the original entry probe bound effects1/46/62 from the selected
corridor and graveyard assets. Corridor entry succeeds. The graveyard initial
script now stops at145 (bhEffModeSet), offset214; its later variant reaches34
(bhEneSetCk), offset214. The prior47/67 boundaries are handled without skipping.
Evidence: local/room-effects-entry-audit.log. The subsequent source-entry
increment handles145/34 and common23/46; current initializer boundaries and
verification are recorded in native-room-entry-status.md.

A separate, explicitly isolated startup/control diagnostic binds both original
graveyard tables before enemy startup and processes ten root/body shadows per
variant using their actual cached poses. This validates allocation/control,
**not successful graveyard initialization, visible shadows or full gameplay**.
Original modifier model/texture loading, modifier-volume drawing, additional
effect callbacks and original-game reference comparison remain required.
Nonzero authored effect links and the absent O_WORK pyb ground-cache branch
currently reject explicitly; the selected tables contain only unlinked records.

Focused native record/binding/room-entry and shadow/control/zombie-startup tests
passed6/6 after their reproducing RED runs. The inactive-owner ground-order
regression also passed RED→GREEN. The final build/package passed91/91 headless
checks. A fresh packaged dummy smoke completed241 native ticks and240 renders,
generated192800 audio frames, submitted184000 and left the menu queue empty.
The ZIP contains only the EXE, README and four license notices. Final original
entry/control evidence is local/room-effects-entry-audit-final.log;
full build/smoke evidence is local/native-room-effects-final-build.log and
local/native-room-effects-final-ui-smoke.log.

One fresh read-only review inspected all five focus areas and found no Critical
or Important issues. Deferred Minor: distinguish the source256-entry authored
efid table from the512-entry runtime work pool. The current decoder accepts up
to512 authored records with bounded native storage; the selected original tables
use1/46/62. A future257-record rejection/retained-map regression should tighten
this authored limit. This review does not establish complete Step1 acceptance.
