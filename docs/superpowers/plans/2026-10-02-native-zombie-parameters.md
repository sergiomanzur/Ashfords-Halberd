# Native zombie parameters implementation plan

Continue the complete Step 1 scope. Zombie startup requires original
personality selection, health randomization and walking choices before its
model, linked-part, motion, collision and AI updates can run.

1. Share the USA ELF's already verified RNG between pickup timing and zombie
   startup. Preserve wrapped 32-bit state and the 31-bit return.
2. Port en01.c's personality/model lookup and normal/easy health tables, including
   the original 30-entry loop bound. Verify percentage rounding and RNG state.
3. Port bhEne01_GetWalkMotion: first zombie consumes one random choice; later
   unlinked active zombies cycle from the preceding root. Preserve ownership
   state on errors and reject missing prior initialized work explicitly.
4. Port all 16 InitType table entries, including root versus cloned-body
   health, extended flags and motion selection. Keep the surrounding startup
   allocation, rendering, collision and AI explicitly pending.
5. Exercise these parameters on both retail graveyard variants in original
   authored order, alongside the existing linked-work checks. This diagnostic
   does not substitute for complete initialization or an enemy encounter.
6. Run relevant tests, full headless build, retail runtime and packaged smoke,
   review source fidelity, record pending controllers and commit.
