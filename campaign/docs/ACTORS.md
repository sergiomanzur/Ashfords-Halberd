# Actors

## Summary

* All characters and enemies are `BH_PWORK` (0x580 bytes): the player is `ply` (`plp`), enemies
  `ene[128]`. A model is an `ML_WORK`: a flat array of `NJS_CNK_OBJECT` nodes in depth-first
  order plus one `O_WORK` (matrix) per node. Motions (`MN_WORK`) are per-frame keys, one track
  per node, bound to the skeleton **purely by node index**.
* The four player files (Claire, Chris, Steve, Wesker; Claire's second costume) share **the same
  22-node hierarchy**. Their motions carry root position and rotations only, so each body keeps its
  own bone lengths and any of the meshes plays any other's motions.
* Enemy models, motions and textures are **not** in global files: each room file carries copies
  for its own records. Other types are loaded from another room file (the F2 spawner,
  [ENEMIES.md](ENEMIES.md) "Enemy spawner (F2)").
* Any enemy or NPC model can be shown as the player ("You appear as"): a display actor drawn in
  place of the player, retargeted from the player's skeleton.

## Memory and load pipeline

### Main arena

* `main.c njUserInit`: `freemem = syMalloc(12845056)` (12.25 MB). Vertex/view buffers live at
  the top; `sys->endp = njpmemp`.
* `system.c bhFirstGameStart`: `sys->obwp` (32 `O_WRK` objects, `[0..3]` reserved for the
  player's weapons/hair) and `sys->itwp` (32 `O_WRK` room items).
* `bhSysCallMonitor` mode 1 (game start / load): reads sys file 1 (common data), then
  `bhInitPlayer` allocates the **persistent player buffers** and sets `sys->mempb = sys->memp`.
  Everything above `mempb` is per room and discarded at every room change.
* Allocators: `pwksub.c bhGetFreeMemory` (64-byte aligned bump from `sys->memp`, refuses only
  beyond `freemem + 12.25 MB`) and `eneset.c bhEne_CallocWork` (refuses beyond
  `mempb + 0xC40000`); on the PC both limits grow by `PC_ARENA_EXTRA`. **Neither checks
  `sys->endp`**: an overrun writes into the vertex buffers. Nothing is freed before the next room.

Player buffers (`player.c bhInitPlayer`):

| Pointer | Size | Content |
|---|---|---|
| `sys->plmdlp` | 131072 | player model block (all sub-models) + their `O_WORK` arrays |
| `sys->wrmdlp` / `wlmdlp` | 32768 each | right / left hand weapon model |
| `sys->plmthp` | 12288 | 512 `MN_WORK` (motion headers), `plp->mnwPb` |
| `sys->plbmtp` | 393216 | base motion data |
| `sys->plwmtp` | 65536 | weapon motion data (motions 100+) |
| `sys->plzmtp` | 8192 | per-motion forward translation (`player.c bhGetTransZ`) |
| `sys->plexwp` / `plhdwp` / `pletcp` | 124 / 124 / 32768 | `plp->exp0`, `exp1` (head/eye), `exp3` |

### Room load

`system.c bhSysCallMonitor` mode 4: `rm_SRRC.rdx` (stage, room, case) is read to `endp - size`
and `Expand`ed to `sys->memp`; `room.c bhSetRoom` realizes the room model, **every enemy
record's model** (`bhSetEneMdl`), objects and items and builds the texture upload list (max 256,
one `bhSetMemPvpTexture` per frame); `bhFinishRoom` adds enemy motion sets (`bhSetEneMtn`), the
room motion set (`sys->rmthp`), `O_WORK`s and the player placement. `sys->memp` is left at the
start of the texture section, which is reused after upload. A story character switch
(`cb_flg & 0x80`, op 137 `bhPlayerChangeSet`) reads the new part and weapon files here. Then
`bhInitEvent` starts the room script and the room's sound bank is requested. The largest
expanded room file is 8.9 MB.

### Textures

## Models and skeletons

### Structures

* `ML_WORK` (0x18): `flg` (`0x80` = chunk model; `0x200` = this instance owns the textures),
  `obj_num`, `datP`, `objP` (first `NJS_CNK_OBJECT`), `texP` (`NJS_TEXLIST`), `owP`
  (`O_WORK[obj_num]`).
* `NJS_CNK_OBJECT` (0x34): `evalflags, model, pos[3], ang[3], scl[3], child, sibling`. `pos/ang`
  are the **live pose**: motions overwrite them every frame.
* `O_WORK` (0x50): `flg` + world matrix `mtx[16]`. `flg` bits: `0x1` matrix set externally
  (`bhCalcTree` keeps it), `0x2` motion does not write this node's rotation (head look, aiming),
  `0x4` node has no motion track, `0x8` blend in YZX order (player upper arms 7 and 11).
* `BH_PWORK`: `mdl[16]` (sub-models), `mdl_n`, `mlwP` (the one drawn/animated), `skp[16]` (skin
  per sub-model), `mbp[16]`/`txp[16]` (copies of `objP`/`texP`), `mnwP` (current motion set) /
  `mnwPb` (own set), `mtn_no`, `frm_no`, link fields `lkwkp`, `lkono`, `lox/loy/loz`,
  `clp_jno[8]` (view clipping nodes), `cpcl` (hit capsules). `O_WRK` (objects, weapons, effects)
  has the same layout up to `0x3E4`, so the model functions take either.

### Binary format and hierarchy

`binfunc.c bhMlbBinRealize(bin, mlw)`: header `char[3]` + status byte, `u16[2]` header size,
`u16[3]` `obj_num`, `int[2]` texlist offset, `int[3]` object offset (relative to `bin + header
size`, `-1` = none). Objects are **one contiguous array**, relocated in place (a buffer can be
realized only once). `bhMnbBinRealize`: `NJS_MOTION` and per node an `NJS_MDATA2` pair (position
keys / rotation keys), plus optional per-frame attributes (`atrP` -> `mtn_attr`).

`MdlPut.c bhCalcModel`: base matrix = translate(`px..`) * rotateXYZ(`ax..`), or with `flg & 0x80`
the linked parent's node matrix `lkwkp->mlwP->owP[lkono].mtx` * translate(`lox..`) * rotate.
`bhCalcTree` walks the **flat array in order** (push on sibling, pop without child), so array
order is depth-first tree order and parents are implicit.

### Drawing

`MdlPut.c bhPutModel`: chunk models with skin data go through `njplus.c npCalcSkin` (or
`npCalcSkinFM` with `mdflg & 0x100`), which blends skinned vertices from the node matrices, then
`bhJumpCnkDraw[draw_tp]` draws each node's model with `owP[i].mtx`; mirror rooms
(`st_flg & 0x100`) add a mirror pass. Skin data is converted once against the model's own bind
pose (`npSkinConvert`), so it belongs to its model. `game.c bhAllDrawModel` draws room, enemy
objects, items, effects, enemies, then the player (unless `stflg & 0x1000000`). The shadow is a
blob, not model-dependent.

### Player part file

Sys AFS partition, file `ply_id + 10 + costume*4`, read by `dread.c bhReadPlayerData` (input at
`sys->memp`):

```
u32 size; model block      -> copied to sys->plmdlp
    repeated: u32 n, n bytes (an MLB, or "SKIN" skin data for the next MLB); u32 -1
u32 size; motion block     -> copied to sys->plbmtp, realized into sys->plmthp[0..]
    repeated: u32 n, n bytes (an MNB; n = 0 means an empty slot); u32 -1
u32 size; z-translation    -> copied to sys->plzmtp
per sub-model with a texlist: u32 size (bit31 = 32-byte align), PLI/TIM2 data -> bhSetMemPvpTexture
```

`O_WORK` arrays are placed behind the model data inside `plmdlp`; `sys->hd_pos = objP[5].pos`.
The loader does **not** clear `plp->skp[]`.

| File | Who | Sub-models (`mdl[i]`: nodes) | Motion slots / used |
|---|---|---|---|
| 10 | Claire (`ply_id` 0) | 0: 22 skinned body, 1/2: texlist only, 3: 22 untextured, 4: 4 skinned (hair), 5: 1, 6: 2, 7: 5 skinned | 111 / 60 |
| 11 | Chris (1) | 0: 22 skinned, 1/2: texlists, 3: 22 untextured | 105 / 55 |
| 12 | Steve (2) | as Chris | 105 / 56 |
| 13 | Wesker (3) | as Chris + 4: 1 node (sunglasses) | 106 / 55 |
| 14 | Claire, costume 1 | 0..3 as above, 4: 4 skinned (hair) | 107 / 60 |
| 15..19 | other costumes | 11-byte placeholders | |

* `mdl[0]` (drawn): **one skinned mesh on node 1**; the other nodes are bones without geometry.
  `mdl[1]`, `mdl[2]`: texlists for eye blink (`plp->mlwP->texP = plp->txp[PlyEyeTab[..]]`).
* `mdl[3]`: untextured copy with a rigid mesh per node, for line/ray hits against the player
  (`bhCollisionCheckL2MDL`).
* `mdl[4]`: Claire's hair / Wesker's piece: object `sys->obwp[2]` linked to head node 5 and
  simulated by `player.c bhCalcHair` (only `ply_id` 0 and 3).
* `mdl[6]`/`mdl[7]` (Claire): used by the moth egg (id 27: `plp->mdl[6]`, `plp->skp[7]`, player
  motions 93..95).

### The player skeleton

Identical in files 10..14:

```
0 -> 1 -> 2 -> 3 -> 4 -> 5                spine, neck, head
               3 -> 6 -> 7 -> 8 -> 9      right arm  (6, 10 are siblings of 4)
               3 -> 10 -> 11 -> 12 -> 13  left arm
          1 -> 14 -> 15 -> 16 -> 17       leg        (14, 18 are siblings of 2)
          1 -> 18 -> 19 -> 20 -> 21       leg
```

| Node | Role |
|---|---|
| 0 | root (root motion position, `clp_jno`) |
| 1 | pelvis, carries the body mesh |
| 2, 3 | spine, chest |
| 4 | neck (procedural look) |
| 5 | head (`hd_pos`, camera look-at, hair link) |
| 6..9 | right arm: shoulder, upper arm, forearm, hand (right weapon `lkono = 9`) |
| 10..13 | left arm (left weapon `lkono = 13`) |
| 14..17, 18..21 | legs (`PlyLegRoute`); 17 and 21 = feet (footstep sound/splash) |

`PlyFlip` (`player.c`) is the left/right map for mirrored motions (`mtn_md & 2`). Bone offsets
differ slightly per character; `PlyInfo[ply_id]` holds collision radius/height (Claire 16.5,
others 17.5).

### Attachments

* Weapons: `sys->obwp[0]` (right, node 9) and `[1]` (left, node 13), `flg 0x81` (linked),
  `weapon.c bhSetWeapon`; grip angles `WpnAngTab`, muzzle positions `WpnTab`.
* Enemies link to the player or to each other the same way (`flg & 0x80`, `lkwkp`, `lkono`).
* View clipping uses `clp_jno` (player 0, 5, 9, 13).
* Hard-coded player node numbers appear in ~40 places (`player.c`, `pl_evt.c`, `cut.c`,
  `weapon.c`, enemy grab code): 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15, 17, 19, 21.

### What makes two models animation-compatible

1. **Same node order and parentage**: motions address node `k` by position `k`.
2. **Node count >= track count**: `SetMtn*` writes `objP[k]` for every track with no bound check
   (memory corruption otherwise). Player motions have 17..22 tracks.
3. Similar bind orientation per joint (rotations are absolute local Euler angles). Proportions may
   differ: player motions carry no position keys below the root.
4. For the player: nodes 5, 9, 13, 17, 21 must be head, hands and feet, and the file should
   provide `mdl[1..3]`.

## Animation

### Playback

`Motion.c bhSetMotion(ew, add, mode, flip_tbl)`, once per 30 Hz tick, followed by
`bhCalcModel`:

* `frm_no` is 16.16 fixed point; `add` (`mtn_add`, usually 65536) advances it; wrap returns 1
  (end) or -1 (start). `mode & 1` ping-pongs.
* Track writers: *Fast* (default: root position, rotations for all nodes), *Normal*
  (`flg & 0x10`: position keys for every node that has them), *Slow* (`flg & 0x20` or `mode & 8`:
  linear interpolation by the fraction of `frm_no`).
* Blending ("hokan"): while `hokan_count > 0`, each node moves towards the target by
  `(65536 - hokan_rate) / 65536` per tick. Changing motion = set `mtn_no`, `frm_no = 0`,
  `hokan_count`, `hokan_rate`.
* `mode & 2` mirrors through `flip_tbl`; `atrP[frame]` -> `mtn_attr` (per-frame events).

### Where motion sets come from

| Set | Pointer | Source | Used for |
|---|---|---|---|
| Player base | `plp->mnwPb` = `sys->plmthp` | part file | walk/run/aim/damage/... |
| Player weapon | `plp->mnwP[100..]` | weapon file `wpnr_no + ply_id*30 + 20` (`dread.c bhReadWeaponData`) | stances and firing; weapon 0 files (and Claire's weapon 1) have no motions |
| Room | `sys->rmthp` | room file `hed02` after the enemy entries | event acting of the player and of NPCs |
| Enemy | `ep->mnwPb`, shared per type `sys->emtp[id]` | room file, one entry per record | enemy actions |
| Player in a grab | `plp->mnwP = ep->mnwP` | inside the enemy's set | the player is animated from the enemy's set, chosen by `ply_id` (zombie `{400, 425, 450, 425}`, bat 30..38, ...) |

### Player motion selection

`player.c bhControlPlayer` -> `bhCtrPly_mode0[mode0]` -> handlers `bhCPM2_act_*`. Locomotion
numbers come from `PlMtnAct[wpntp][dmlvl][k]` (`wpntp` 0 below weapon 10 else 1; `dmlvl` health
level fine/caution/danger; e.g. standing = 42/43/44); weapon stances `PlMtnWpn = {100, 104, 109,
114, 101}`. Numbers are shared by all four characters.

Enemy code sets `ep->mtn_no`, `hokan_*`, `mtn_add` and calls `bhSetMotion` itself. The first
record of a type in a room carries the set, later records reuse it; children often share the
parent's set.

## Player mesh replacement

### Option A

1. Save the original's `mdl[]`, `skp[]`, `mbp[]`, `txp[]`; clear `plp->skp[]` (the loader would
   apply stale skin pointers).
2. Read the part file and run the game's loader (`read_player` -> `player_model_read`, the
   `bhReadPlayerData` layout) with the model going into a buffer of its own (`self_mdl`, 128 KB)
   and the motions into a sink; restore `plp->mnwP`/`mnwPb`, so the original motions (incl. 100+)
   stay. The original model data in `sys->plmdlp` and its textures stay loaded.
3. Sub-models only the original has (Claire's 5..7: the moth egg takes `mdl[6]`, `skp[7]`) keep
   the original's; `owP[7]`/`owP[11]` get `flg |= 0x8` as in `bhSetPlayer`; the hair/sunglasses
   object is set up for the new mesh. Putting the original back copies the saved `ML_WORK`s.

## Enemies

### Data flow

Room file records (`ETTY_WORK`/`EGG_WORK` 0x24: `flg`, `id` (type), `type` (variant), `flr_no`,
`mdlver` (model version), `wrk_no` (slot), position, angles, `hide[4]`) are realized by
`room.c bhSetRoom` -> `bhSetEneMdl` -> `eneset.c bhSetEnemy` (first free slot). Model chunks:
`"MASK"` -> face mask `mskp`, `"SKIN"` -> `skp`, else an MLB into `ep->mdl[]`. Textures are shared
per `et_lp[id][mdlver][k]`; the first record owns them (`flg |= 0x200`). `bhFinishRoom` adds the
motion set (`sys->emtp[id]`), `O_WORK`s and `bhInitMask` for ids > 90.

### Enemy types

`bhJumpEnemy` (100 entries, `eneset.c`); code in `enNN.c` unless noted, names from the
`// ENEMY:` comments:

| id | Name | id | Name |
|---|---|---|---|
| 0 | exploding static object, wall collider (`eneset.c bhEne00`) | 17 | Monster Steve |
| 1 | Zombie (16 variants; parts as helpers 31..38) | 18 | Alexia's Tentacle |
| 2 | Gulp Worm | 19 | Tyrant T-078 |
| 3 | Black Widow (helpers 31, babies 24) | 21, 22 | Albinoid Infant, Adult |
| 4 | Cerberus | 23 | Giant Black Widow |
| 5 | Hunter/Sweeper | 24 | Baby Black Widow |
| 6 | Moth (lays 27) | 25 | Ceiling Trap |
| 7 | Bat | 26 | Anatomist Zombie |
| 8 | Fish | 27 | Moth Egg (on the player; player model/motions) |
| 9 | Bandersnatch | 28 | empty (`subpl.c`) |
| 10, 20, 71 | Ant A, B, C | 29 | Tentacle |
| 11 | Spotter | 30 | Alexia's Baby |
| 12, 13, 14 | Alexia first, second (spawns 18), third form | 53 | Nosferatu's core (`en15.c bhEne53`) |
| 15 | Nosferatu | 54, 55 | Armored Knight A (sword), B (lance) |
| 16 | Alfred Ashford | 56 | scripted object (`eneset.c bhEne56`) |

31..52, 57..70, 72..99: `subpl.c bhSubpl`, scripted NPC actors (room-script motions, no AI);
41..48 and 91..98 get player-like clip tables, ids > 90 face masks and lip sync.

Slots 31..38 are overwritten at run time with helper functions (`bhEne_SetCallFunc`): 31 by ids
2, 3, 5, 6, 13, 23 (17: 31, 32), 31..38 by id 1. On the PS2 the table is global and never
restored; the PC runs each part with its creator's function in rooms with added enemies
([ENEMIES.md](ENEMIES.md) "Budgets on the PC"). Ids 18, 27, 28 never appear in room data.

### Cloning a type the room already loaded

`bhSetEnemy` on a copy of an existing record of the type, then share its `mdl[]` (clear
`flg & 0x200`), give each sub-model its own `owP` (`bhEne_CallocWork`), copy
`skp`/`mbp`/`txp`/`mskp`, set `mnwP = mnwPb = sys->emtp[id]` and `bhCalcModel`; Init runs on the
next tick. Children share `objP` the same way (`en03.c`). Foreign types go through the spawner's
loader ([ENEMIES.md](ENEMIES.md) "Enemy spawner (F2)").

### Items and flags (for randomizers)

* Items: room records `rom->itmp` -> `sys->itwp[i]` (`objitm.c bhSetItem`, `id` = inventory id
  0..155). Pickup: an `etc` atari of type 4 whose `prm0` is the item slot (`sys->sb_id =
  itwp[prm0].id`). Ops 14 `bhItmCk` / 35 `bhItmSetCk` bind the `it_flg` bit to the trigger, not the
  id, so changing `itwp[i].id` after load keeps persistence (the 3D model stays the record's).
* Locks are room-script checks (ops 16 `bhUseItemCheck`, 17 `bhPlItemCheck`, 77
  `bhPlItemBlockCk`, `ky_flg`); `map.c ItmDat` lists 22 key items. Flags (`SYS_WORK`): `ev_flg`
  story, `it_flg` items, `ed_flg` dead enemies, `ky_flg` locks, `mp_flg` map, `ic_flg` item checks.

## You appear as

### The list

Left out of the menus (`twins[]`):

| Hidden (catalogue index) | Shown instead | Why |
|---|---|---|
| Chris (event model) 2 | Chris | same look |
| Steve (event model) 4 | Steve | same look |
| Wesker (event models) 6, 7 | Wesker | same look; the doubles wear the sunglasses as a separate prop |
| Alfred Ashford 2 (9), (event model) 11 | Alfred Ashford (8) | same uniform |
| Tyrant 2 (18) | Tyrant (17) | one extra blood stain |
| Bandersnatch 2 (23) | Bandersnatch (22) | only its long arm (event piece) |

Kept although related: "Alfred Ashford (wounded)", "Hunter (Sweeper)" and every zombie variant.
Order: As played, the player files, the cast, the creatures, the zombies, then the cutscene models
of Claire (0), Chris (1), Steve (3) and Wesker (5) (`tail[]`): listed next to the player files,
each would follow the same character's file. They are the event doubles' meshes: no hands (the
unarmed pieces are drawn, as for the rest of the cast), and no head part of their own (in
cutscenes the scripts link the player's `sys->obwp[2]` to the double); the display actor brings
Claire's ponytail and Wesker's sunglasses itself ("Head parts" below). Earlier versions hid them as
twins of the player files; their stored values stayed valid.

### Head parts

* **Source**: sub-model 4 of the character's part file (`lkmtab`: 1211 the ponytail, 1212 the
  sunglasses; Claire's costume 0, file 10, and Wesker's, file 13), read when the model is loaded,
  whatever character is played: only its skin and model chunks are kept (Claire 1.4 KB, Wesker
  2.3 KB, into the display slot's own buffer) and only its texture block is uploaded (Claire 128 KB,
  Wesker 5 KB, one texture entry; refused like a model under the margins of "Loading"). The
  player's own model and `sys->obwp` are not touched; the part file goes through the loader's
  `pack` buffer.
* **Set up** as `bhSetPlayer` does (`player_hair`: offsets on head node 5, Claire's skin), in an
  `O_WRK` of the display slot, linked to the display actor's work (all four cast models have their
  head at node 5, which `bhObjClpn` reads). Per pose its position follows the model's head; the
  ponytail runs the game's own simulation (`bhObjClpn`, `bhCalcHair`: the same joints, from the
  display actor's retargeted angles) once per tick (our player and the partner: the pose after
  control; the double: `actor_double_tick`; of two doubles shown the first steps it, the other
  shows the same swing), in work of its own (`exp0`/`exp3`, 20 KB per slot,
  flag `0x100000`), so it never takes `bhGetFreeMemory` (the earlier crash: memory under a pending
  weapon read). Drawn right after the display actor.
* **Memory**: per display slot (3) 34 KB of static buffers (103 KB in all, not the game's arena); per shown head part one texture
  entry and the texture above; one part-file read (about 1.2 MB, the existing buffer) per model
  change. Released with the model (`unload`, `njReleaseTextureAll`: game over, the title).
* **The played character's own cutscene model** (Claire's while Claire is played): the doubles
  stay the game's (see "Cutscene doubles"), and while one acts `sys->obwp[2]` is the character's
  own head part linked by the script (`self_cine_hair`, as with "Swapped model in cutscenes" off,
  `actor_double_kept`); in gameplay the display actor's copy is shown.
* Not covered: a script's InitPony (`flg2 & 0x2`) does not reset the copy; a teleport swings it once.

Models not in the catalogue:

| Model (id) | Why |
|---|---|
| Alexia second form (13) | the body is one of 14 linked sub-models |
| Alexia third form (14), Gulp Worm (2), Tentacle (29) | arena-sized or below the floor |
| Armored knights (54, 55) | armour pieces scattered on the floor in their idle motions |
| Alexia's baby (30) | a thin stalk in motions 0 and 1 |
| Ant A (10), Ant C (71), baby Black Widow (24), Nosferatu's core (53) | tiny, no model, or nothing visible |
| NPC 68 | a black figure without a head |
| ids 0, 16, 25, 56, 58..63, 65..67, 69, 70, 72..79, 81..85, 87..90 | props and set pieces |

### How

### Gender-matched animations

Setting "Use gender-matched animations" (APPEARANCE page, ini `appear_gender_anim`, default 1): a man
shown while Claire is played moves with **Chris's** motions; a woman shown while a man is played
moves with **Claire's**. Gameplay stays the played character's.

The catalogue's `anim` field picks the set (`appear_motion_chr(anim, ply_id)`; player files by
their file, `appear_part_anim`):

| `anim` | Models |
|---|---|
| `ANIM_MALE` | Chris, Steve, Wesker (all files and models), Alfred (all), Rodrigo, the hazmat man, Nosferatu, Tyrant, Monster Steve, Hunter, Bandersnatch, the anatomist, every zombie |
| `ANIM_FEMALE` | Claire (player files, cutscene model), Alexia Ashford, Alexia's first form |
| `ANIM_IDLE` (own idle) | Cerberus, albinoids, Black Widows, moth, bat, fish, Spotter, ant |

### Retargeting

Each model keeps its own skeleton: bone lengths, rest pose and proportions are its own, only the
joint rotations come from the source (`actor_glue.c rig_build`, `retarget`).

* **Rest poses**: computed once at load from the model's node tree (`rig_build`: parents and
  model-space rest positions; all bind angles are 0, so every rest orientation is the identity, as
  the player's). The source's rest pose is its current node offsets (the player files' bones).
* **Rotations**: per mapped joint, the model's rotation is the source's rotation relative to its rest
  pose, turned by a rest correction: the rotation taking the model's rest bone (towards the next
  mapped joint of the chain) onto the source's. In local terms: model local = C(node) x source local x
  C(parent)^-1, so a model whose arm rests at another angle (an arm lower or more forward, a hunched
  spine) bends at the same joints by the same amounts without being pulled to Claire's bone lengths.
  A chain end (hand, toe) takes its parent bone's correction. Node angles are written back as
  `njRotateXYZ` builds them, so the skin blends joints (`npCalcSkin`).
* **Limb roles**: an arm runs from its first node sideways from the chest to the hand: the last node,
  or the first with two or more children whose subtree is short against the arm (fingers, claws; a
  spike on the Tyrant's forearm is passed). Clavicle, shoulder and elbow are the nodes nearest to where
  the player has them along the arm (3%, 35% and 68% of its length from the chest), in order, so a
  long arm (the Bandersnatch's six joints) has its hand at its claw and an arm without clavicle
  (Hunter) maps from the shoulder. A leg of more than four joints (Monster Steve) keeps hip, toe and
  ankle at its ends and the knee at mid-leg. Joints between mapped ones follow their parent rigidly.
* **Root and feet**: the root's height over the floor is scaled by the hips' heights (crouching,
  kneeling, the walk's bob), and the body is then moved up or down so that its lowest ankle or toe is
  as far over its rest height as the source's (in proportion; at most 3 units): feet of another shape
  (a Hunter's long foot, Monster Steve's claw) stay on the floor. Only the height: room motions move
  cutscene doubles across the room with the root.
* **Post-passes**: the two-handed grip IK and the lighter IK (below), the face mask's jaw, tongue and
  eyes, the cutscene doubles' hand pieces, as before.
* **A missing hand**: what the game links to a source hand the model does not have (Nosferatu's right
  hand, the Bandersnatch's left) is shrunk to a point instead of floating where the source's hand is.
* **Free aim** ([THIRDPERSON.md](THIRDPERSON.md) "Free-aim pose"): the bend is solved on the weapon as
  drawn (`actor_aim_hand`; only while the aim pose stands still, eased, not for the knife; a solve
  that stops converging, a target a few units from the muzzle, keeps its best step), and the third-person camera's height, shoulder offset and distance scale
  with the model's head height over the character's (`actor_cam_scale`, 0.8 .. 1.8): the camera is no
  longer inside a Tyrant or Monster Steve.
* **Previous retargeting**: `RECVX_RETARGET=0` (environment, read at start) keeps the earlier mapping
  (fixed chain positions, clavicle by length), the unscaled root and the aim taken from the unbent
  hand; frames are then identical to the previous version's. The character played without a
  model never goes through this code.
* Test aid `RECVX_RIG_DUMP=1`: logs each model's rest skeleton, the source's map and skeleton, and per
  tick the source's and the model's lowest foot over the floor.

Why aiming up looked broken before: the free-aim bend took its pitch from the hand of the unbent pose
and turned the upper body about the chest, which swings a hand far from the chest (a tall model's)
off the line (a drawn barrel up to 11 degrees off a near target); the camera stayed at Claire's
height, inside tall models; the old map took Monster Steve's clavicle for his upper arm (his weapon
at his elbow, his feet 4 units over the floor) and the Bandersnatch's mid-arm for its hand; the
weapon of a hand the model lacks floated at the invisible source's hand.

### Hands, joints, lighter and aim

* **Weapon hands** (`actor_pose`): the source's hand nodes 9/13 get the model's hand position and
  the copy's hand rotation for the tick's effects, objects and drawing, so weapons sit in the
  model's hands and the muzzle flash starts there.
* **Two-handed grips** (`retarget`, `arm_ik`): when the source's left hand is near its right
  (closer than 6 units, full below 3), the model's left hand gets a target at its right hand +
  (source left - source right), solved by a two-bone IK on the model's own bone lengths (elbow
  kept in its bend plane). One-handed poses are untouched.
* **The shot** (`actor_hands_back`, game.c before `bhControlPlayer`): the game's own matrices of
  nodes 5, 9, 13 are saved before they are moved and put back before the next player control, so
  the game fires (`bhCheckGunAtari` from the muzzle at `owP[9]`), swings the knife and places the
  lighter light from the real skeleton; only the drawing uses the model's hands. Third-person aim:
  [THIRDPERSON.md](THIRDPERSON.md) "Free-aim pose".
* **Lighter**: the game holds the lighter by an arm IK after the motion (`bhArmIkMdk` on node 6
  towards a point in front of chest node 4, nodes 7..9 flagged 2). The same IK runs on the
  gender-matched copy whenever the source's arm is flagged (our player, and the partner via
  `lighter_arm`).
* Known: the Tyrant's weapon is lost inside its big claw hand (it sits at the wrist node); in
  Chris's two-handed stance the left palm lies under the gun on every male model (the motion's
  own grip).

### Cutscene doubles

### Voices

The player's vocal sounds follow the model shown: hit reactions, death, grab and struggle
screams, and what room scripts play as the player's voice (not cutscene dialogue, which is
streamed ADX).

**Where they come from.** Every player vocal sound is SE bank 4 (`0x400 + program`) through
`sdfunc.c CallPlayerVoice` (slot 7): damage and death (`player.c bhCPM0_damage`, `bhCPM0_die`),
enemy grabs and room scripts (`PlayGameSe4Event` type 0). Bank 4 is `CORE_<ply_id>.SPQ` (AFS key
`0xFFF0 | ply_id` in MULTSPQ1/2); the disc has exactly four (Claire, Chris, Steve, Wesker) with the
same 4 programs each. No other character has player vocal sounds.

**Mapping** (`actors.c appear_voice`, ctest `appear`):

| Shown as | Voice |
|---|---|
| As played | the character played |
| Claire (all files and doubles) | Claire (`CORE_000`) |
| Chris and his doubles | Chris (`CORE_001`) |
| Steve and his doubles, Monster Steve | Steve (`CORE_002`) |
| Wesker and his doubles | Wesker (`CORE_003`) |
| other men (male `anim`): Alfred, Rodrigo, the hazmat man, Nosferatu, Tyrant, Hunter, Bandersnatch, the anatomist, every zombie | Chris |
| women (female `anim`): Alexia, Alexia's first form | Claire |
| creatures with their own idle | the character played |

Creatures keep a human voice: enemy sounds are motion- and table-driven per type, list numbers
collide between types ([ENEMIES.md](ENEMIES.md) "Sounds of added types"), none is labelled hurt or
death, and they play through the enemy sound slots.

**Memory.** A voice bank is 0.7 KB header + 38..54 KB samples, at most two at a time (ours and the
partner's), taken from the same 31 extra-bank numbers as the spawner's enemy banks.

### Limits

## Limits

Budgets for enemies (slots, arena, textures, sounds): [ENEMIES.md](ENEMIES.md) "Budgets on the PC".

| Resource | Limit | Where |
|---|---|---|
| Objects / items | 32 `obwp` (0..3 player), 32 `itwp` | `system.c bhFirstGameStart` |
| Effects | 512 `eff[]`, 450 effect models `sys->efm` | `main.c`, `SYS_WORK` |
| Enemy types | 100 jump entries; textures `et_lp[100][16][16]` | `eneset.c`, `room.c` |
| Motions | 512 per set; 64 tracks per motion (`NormalTbl`), at most the model's nodes | `Motion.c` |
| Sub-models | 16 per actor (`mdl[16]`) | `BH_PWORK` |
| Player buffers | model 128 KB, base motions 384 KB, weapon motions 64 KB | `bhInitPlayer` |
