# Native enemy defeat reconciliation

Original event.c::bhDieCk is now instruction13 in the native scenario loop.
It consumes exactly4 bytes, resolves authored enemies through the allocated
work-slot map, and records work.flags bit2 in persistent flag bank3 using the
original ten-bit/MSB-first index. Already recorded defeats bypass the work map.
Truncated commands or invalid required mappings reject before mutation. Health,
enemy flags, other banks and instruction34's nine-bit behavior are unchanged.

Verification against BASE1f99464 produced authentic reconciliation/reference
RED (local/enemy-defeat-current-red.log). The private harness compiled the
unchanged original function and captured96 source cases. event.c SHA256:
7f0fa411734d385ab1f39dace34862b692f7b2485849eb75557ce94984428cd5.
This is source-derived native scalar evidence, not PS2/original-game acceptance.

All3 focused tests pass (local/enemy-defeat-focused.log), covering48 behavioral
combinations,96 original reference cases, incomplete instructions, mapping bounds
and cached defeat bypass. The complete build passes143/143 tests and installs/
packages (local/enemy-defeat-full-suite.log). A fresh isolated dummy SDL packaged
run passes241 ticks/240 renders,53 movement and10 focus-blocked ticks, generates
192800 audio frames, submits184000 and closes with queue0/Effects100
(local/enemy-defeat-packaged-smoke.log). Exact six-file ZIP/EXE SHA256 and
formatting/whitespace checks pass. No retail data is included in Git or the ZIP.

The unchanged retail entry probe now advances the cemetery15 production ticks;
instruction13 no longer blocks its first tick. It next reaches original173 at
offset4. The corridor still advances120 ticks; cemetery revisit still reaches
139 at276 (local/enemy-defeat-entry-audit.log). No scripts or flags were forced.

The read-only next-boundary audit identifies task2/origin4350, authored effect16
mapped to pool slot16, effect179. Native bound type remains1 and the event
requests1. Actual effsub0.c::bhEff179 consumes type and emits effect180 through
setentry; effect180 controls/draws an animated texture408 spark. This consumer
and its drawing path are still missing. Enabling only a type setter would not
complete the required effect behavior. Record the source controller/emitter,
allocation, animation, texture and drawing dependencies for the next increment.

Full Step1 remains unfinished: two-room interactions/transitions, original enemy
AI/combat and healthy start, inventory, complete decoded sound/effects, original
camera control, versioned cross-room saves and independent original-game
acceptance remain required. New Game stays gated. Source clipping/retained
lighting context limits from the lighting checkpoint also remain pending.
