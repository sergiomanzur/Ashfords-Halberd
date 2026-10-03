# Native zombie work implementation plan

Parent goal remains the complete Step 1 native playable slice. Graveyard entry
currently stops at its required zombie initialization; no controller may be
replaced with inert behavior to make travel appear functional.

1. Port eneset.c::bhSetEnemy work-slot allocation into a reusable native helper.
   Preserve the fixed 128-slot pool, first-inactive search, work reset, authored
   position/yaw/type/model version, special model flags and clipping joints.
   Test reuse, full-pool failure and malformed floor data before implementation.
2. Port en01.c::bhEne01_SetLinkEnemy with stable parent slot references, shared
   immutable model/motion ownership and original hidden linked-body flags.
   Test pool capacity, inherited assets and copies retaining valid references.
3. Reuse native allocation when binding authored room enemies; retain original
   room motion-loading order and authored-to-work mapping.
4. Verify synthetic suites and retail assets headlessly. Then implement zombie
   initialization, linked-part updates, motion/AI/collision and damage from the
   pinned source. These remain required before enabling graveyard travel.
