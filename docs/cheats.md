# Cheats in Ashford's Halberd

Open **F1 > Cheats**, press **F2**, or use **OPTION > ENHANCED OPTIONS > CHEATS**.
These menus share the same native controls. Player actions require an active
game; disabled entries explain when they are unavailable. Health/ammo and
other runtime toggles are session tools. Item grants, unlocks and rank edits
can affect a subsequently saved game; keep a separate save for experiments.

## Research and implementation

The PS2 references were checked on 2026-10-23:

- [GameFAQs PS2 cheats](https://gamefaqs.gamespot.com/ps2/445328-resident-evil-code-veronica-x/cheats)
  documents the campaign rocket launcher reward, Battle Game unlocks and the
  infinite Linear Launcher reward for A ranks with all Battle characters.
- [GameRevolution PS2 cheat list](https://www.gamerevolution.com/guides/30612-resident-evil-code-veronica-x-ps2-cheats)
  lists health, inventory/ammo, item-box grants, extras and rank-related cheats.

This port implements compatible effects directly through its own game state.
PS2 encrypted GameShark addresses cannot be applied to a native PC process.
The item selector covers the individual weapon, ammunition, healing and key
item codes through one grant control. Engine tools also include noclip,
enemy spawning and room teleportation. Teleporting or granting story items
does not solve the campaign's event dependencies automatically.

The historical web lists include conflicting rumors, including a Hunk Battle
character. Unsupported characters and arbitrary console memory patches are
not offered. This is a researched set of native equivalents, not a claim
that every regional cheat-device code has been implemented or validated.
