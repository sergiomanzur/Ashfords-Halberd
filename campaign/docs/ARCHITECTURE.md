# Architecture

## Overview

The game code (gameplay, AI, collision, menus) **and** the original PS2 platform layer are
compiled from the [recvx-decomp](https://github.com/AshfordFamily/recvx-decomp) decompilation.
The decomp's `ps2_*.c` files implement Sega's Ninja/Shinobi API on PS2 hardware: they build DMA
chains and VIF/GIF packets and run VU microcode. Instead of rewriting that layer (and risking
behavioural drift), the port keeps it and emulates what lies underneath, at the hardware and
library boundaries:

```
game logic (decomp)  ->  Ninja/Shinobi on PS2 (decomp ps2_*.c)
                           |-- inline EE/VU0 asm  -> translated C (asm2c) on a VU0 model
                           |-- vcallms            -> VU0 microcode (static C)
                           |-- sceDmaSend/CHCR    -> pc/src/ps2/dma.c (chain walker)
                                 |-- VIF1 -> vif.c -> VU1 microcode (static C) -> XGKICK
                                 |-- GIF  -> gs/gs.c (PATH1/2/3) -> gs_gl.c (OpenGL)
                           |-- libcdvd/libpad/libmc/SIF RPC -> host implementations
```

### Source layout

The build (`pc/CMakeLists.txt`) has three parts: `recvx_game` (the decomp's `prog/*.c` minus a
few excluded files, plus `pc/glue`), `recvx_platform` (`pc/src`) and the `recvx` executable.
The decomp is compiled with flags that keep the PS2's arithmetic and layout assumptions:
`-fsigned-char`, `-fwrapv`, `-fno-strict-aliasing`, `-msse2 -mfpmath=sse -ffp-contract=off`
(IEEE single precision, no FMA, PS2 operation order), `-ftrivial-auto-var-init=zero`, and on
Windows `-mno-ms-bitfields`.

## Data model and memory

- **32-bit build.** Disc data contains 32-bit pointers that are relocated in place, and the
  platform code stores host pointers in 32-bit DMA tag fields. A 64-bit build would need those
  structures to keep 32-bit pointer slots; it is not planned before the 32-bit port is complete.
- **EE `long` is 64-bit**: `pc_prelude.h` includes the host headers, then
  `#define long long long`.
- **Address window**: the executable is linked at `0x30000000` (Linux `-Ttext-segment`, Windows
  `--image-base` with ASLR off), so the EE's uncached/accelerated aliases (`addr | 0x20000000`,
  `addr | 0x30000000`) are identities and DMA tag addresses fit in 31 bits. The 16 KB scratchpad
  is mapped at its real address, `0x70000000`, because the platform code builds packets at
  literal scratchpad addresses.
- **The window is 256 MB**: all static game memory (room arena, texture heap, spawn pool, ...)
  is the image's `.bss` and must end below `0x40000000`; `ps2/memory.c` checks this at start.
  Host allocations (SDL, OpenGL, caches) can live anywhere. The PC raises several PS2 budgets
  within this window (see [ENEMIES.md](ENEMIES.md) and
  [modding/ROOM_FORMAT.md](modding/ROOM_FORMAT.md#limits-and-budgets)).
- String literals are writable (the game writes into them; the EE has no MMU protection).
- File paths are UTF-8 everywhere (`pc_fopen` in `host/fileio.c` uses `_wfopen` on Windows; the
  Windows build rebuilds `argv` from the UTF-16 command line). Settings are saved by writing a
  temporary file and renaming it over the old one.

## Threads and shared state

The game runs single-threaded as on the PS2: interrupts are delivered synchronously at platform
entry points (vblank waits, sync calls), and timing follows NTSC rates.

How state crosses threads:

## Renderer

### Frame stream

The game thread runs the decomp's main loop, the VU code and the GIF parser, and only
**records** each frame's GS command stream (register writes, image uploads, display buffer
changes; `gs/stream.c`). At the buffer swap the frame is queued for the main thread, which
executes it against the GS emulation and OpenGL and presents it in step with the display
(`host/render.c`). At most one frame is in flight. A GS readback (`StoreImage`) is a sync
point: the partial frame is executed and the game waits for the pixels.

### GS emulation

- GS local memory is exact: all pixel-storage swizzles, CLUT buffer and CLD semantics,
  transfers.
- Rasterization is OpenGL 3.3 at `scale` times the native resolution, with one render target
  per framebuffer address. Colours are stored as RGBA16F (rgb = C/255, a = A/128), so GS blend
  factors up to ~2.0 map directly to GL blending. A texture whose base is a render target samples
  that target, so render-to-texture effects (for example `PS2_swap`'s frame blend) work
  unchanged.
- Textures are decoded from GS memory and cached per TEX0/CLUT, invalidated by page write
  generations.

### Widescreen (hor+)

Render targets get GS-pixel margins on both sides. The game's clip matrix
(`_Make_ClipMatrix`) and CPU culling cone (`Ps2CalcScreenCone`) are widened by `pc_hor_scale()`
while the projection is untouched, so 3D fills the margins and 2D drawn at x 0..639 stays 4:3 in
the middle. Full-screen 2D layers are stretched to the widened view as the DISPLAY settings say
(`host.c pc_overlay_begin`, `pc_overlay_x`): cutscene bars `PC_OVL_CINEMA`, fades `PC_OVL_FADE`
(always), screen effects `PC_OVL_EFFECT`, which also covers the scope's lens tint (`screen.c
bhDrawScope`; the scope's view is hor+ as any other, its reticle stays 4:3 in the middle). Model culling is widened the same way (`fNaViwHalfW`, the joint window of
`bhCheckClipModel`/`bhCheckClipPoint`). The bounding-sphere test of every chunk model
(`njCnkDrawModelLocal`: room parts, objects, effects) is replaced in widescreen and in third
person (`view_glue.c pc_model_clip`): the original compares the sphere's on-axis screen radius
(`r * dist / z`) with the screen edge, which falls short away from the axis by
`sqrt(1 + tan^2)` of the angle, so at 21:9 and 32:9 large parts near the camera vanished at the
edges while they still reached into the picture. The sphere is tested against the view's
planes instead, widened by `PC_CULL_MARGIN` (1.5) of the half width and height, the same share
of the view in every aspect; the margin also covers models whose sphere does not hold all of
them (effects near the camera) and parts turning into view. A part only this test draws with a
fixed camera is cut to the margins as below, so the 4:3 picture stays the original (without
the native 3D renderer it stays culled); 4:3 with a fixed camera keeps the original test. Room cameras hide room parts they cannot see (`hidobj`
lists, authored for 4:3). In widescreen such a part is drawn anyway, but only in the margins
(`ps2_NinjaCnk.c pc_show_hidden` -> `view_glue.c pc_margin_part` -> `pc_margin_next`): its
triangles carry `NativeVertex.cull` bit 2 and `stream.c native_draw` cuts them to the parts
left and right of the 4:3 screen (GS x 1728..2368), so the 4:3 picture stays exactly the
original, also for parts that reach across its edge. Parts that would cover the player or end
in a straight edge at the 4:3 border are left out (`native_glue.c pc_margin_reaches_43`, checked
on the model's vertices after its vertex chunk): a part whose bounding sphere reaches the camera
or the line from the camera to the player (a wall the cut looks through, or one running past
the camera) unless all its vertices lie on one side of the 4:3 view widened to take in the
player, so that their hull misses it at every depth (the courtyard walls running from the
camera's corner, 0/3 and 0/10, were missing at 32:9), and other parts nearer than the player
with a vertex inside the 4:3 view. A part next to the camera can so cover some of a margin, as
a wider lens would show it. A part left out takes along the hidden parts below it in the room's
tree whose bounding sphere lies inside its box (of its vertices in view space; `ps2_NinjaCnk.c`
`njCnkEasy*DrawObject` -> `view_glue.c pc_margin_children`, `pc_rules.h pc_margin_inside`): what
it holds, so the dolls on the shelves of Alexia's cabinet (2/5, second cut) no longer stand in
the margin in front of an empty frame. Trees also group parts that only stand beside each other
(a flower pot under a terrace part in 1/0, pillars along a wall in 1/7); those stay. Without the native 3D renderer a hidden part is
only drawn when its bounding sphere lies entirely outside the 4:3 view and away from the
camera. "Cull off-screen models" (`model_cull`) is unrelated: it only skips actors and objects
whose test points are off the (widened) screen. Full-width scissors, fills and render-target
copies are extended into the margins. So 2D drawn outside x 0..639, which the PS2's display never showed, would appear there:
while the Itemselect and Map tasks run (status screen, item box, files, map; `main.c
njUserMain` -> `pc_menu_clip`), their 2D is clipped to the 4:3 screen (`native_glue.c`:
triangles by their clip distances, sprites on the CPU), except stretched overlays and full-width
fills. The OPTION screen's scroll plates are drawn again 1344 pixels to either side
(`adv.c DisplayOptionBg`) so the band reaches the edges.

### Native paths

The emulated pipeline (VU1 microprograms -> GIF -> GS -> OpenGL) is replaced by native paths
where it pays off. Each is switchable at run time (F1 -> Display, `RECVX_NATIVE3D`,
`RECVX_NATIVE_TEX`, `RECVX_NATIVE_RTEX`), and `pc/tools/compare_native.sh` diffs the paths
headless.

- **Native 3D** (`pc/glue/native_glue.c`, `gs/stream.c`, `gs_gl.c`): chunk-model strips
  (`njCnkCsUvh/Uvn`) no longer go to VU1. The per-vertex results of the VU1 programs (colour,
  ST/Q, fog, back-face culling of single-sided strips) are computed on the CPU; the position is
  passed homogeneously, exact to the VU0 `CALCPOINT` formulas, and the GPU projects. Triangles
  are clipped against near/far on the render thread, as the VU1 `SCISSOR` program does.
- **Ordering-table primitives**: the CPU strip functions (`ps2_Vu1Strip.c`) and the general
  builder `Ps2AddPrim` (2D polygons, effects, particles) produce native records too. Their GS
  packets keep the render state but carry, instead of vertices, a write of the record number to
  GS register 0x7F (`NATIVE_MARKER_REG`), so each record is drawn exactly where the GS would have
  drawn it in the sorted order. Sprites, lines, points and the full-screen operations of
  `ps2_dummy.c` (fades, copies) still take the GS path.
- **Native textures** (`pc/glue/texture_glue.c`, `gs/textures.c`): the game re-uploads the
  whole TIM2 to its single GS texture slot on every switch. Each TIM2 content is recorded once as
  a definition and bound on switches. CLUT uploads are executed for real; image uploads are
  deferred and written into GS memory only before something reads or overwrites them (CLUT
  loads, transfers, downloads, non-native TEX0s, render target loads, HD pack keys), earlier
  queued uploads overlapping them first. A queued upload is dropped when a later bind covers it:
  to the same slot and not larger, or anywhere within the whole pages the later image overwrites
  (an upload from (0, 0) as wide as its buffer to a page-aligned slot, not a 24-bit or high-bits
  format). Nothing read it in between, so uploads queued in between that overlap it end up under
  the later one either way. The queue stays short (about ten entries), the game's switches write
  nothing, and a forgotten definition writes only its own upload if it is still queued. Replays
  never write GS memory. Memory is
  capped (definitions 96 MB, decoded textures 256 MB). Texture identities are kept in an 8-way
  set-associative table.
- **Render textures on the GPU**: the game's only GS readback is `StoreRenderTex` (full-screen
  distortion, the stage-7 small screen, `Ps2TextureMalloc` textures). The game thread records a
  capture event instead; the render thread blits the render-target area into a texture that
  becomes a native texture definition, and the TIM2 only receives a 16-byte marker. A stale marker
  falls back to a plain upload. In widescreen the full-screen distortion captures the widened view
  (`pc_rtex_wide`) and draws it back over the whole view. The other small screen (the Palace
  monitor, `PS2_Render_Tex_Sub`) is a GS copy into FBP 476 sampled as a texture; a target's drawn
  area is its scissor cut to the vertices of 2D batches, so a CLUT upload in the pages below that
  copy does not make it stale.
- **HD texture packs** (`gs/hdpack.c`, [HD_PACK.md](HD_PACK.md)): every texture decode computes
  PCSX2's replacement key (XXH3 of the texture's GS memory and palette); a file of that name in
  `mods/textures` is checked against the original once and then bound in its place, in the same
  UV space. Three loader threads read and check the files; the render thread uploads a few ms of
  them per frame, and waits for them only under a screen fade (`Frame.fade`, a room's fade-in).
- Still emulated: the GS register/state model, GS memory (render-to-texture, CLUTs, non-native
  textures), render targets, VIF/DMA for the remaining packets.

### Enhanced mode: in-between frames

Game logic, physics, animation and audio stay at the original 30 Hz: they are frame-stepped
throughout the game code, so running them faster would change gameplay. Instead, the renderer
shows in-between images at the display rate:

- The engine tags every 3D mesh it draws (`njCnkDrawModelLocal[Mod]` -> `pc_mesh_begin/end` ->
  `stream_tag`) and every strip (`pc_mesh_strip`), which gives exact vertex correspondence
  between consecutive frames. Of several instances of one mesh, the one that moved least is
  matched. TEX0 is not part of the key (textures stream through rotating GS memory).
- The game's clip volume reaches into the GS guard band (`pc_clip_guard`), so geometry crossing a
  screen edge is not re-cut differently every frame.
- At each display refresh the executed frame's whole GS stream is **replayed** with matched
  vertices blended in homogeneous space (x*w, y*w, w = 1/Q; S/T/Q rescaled so textures keep their
  perspective). For a display rate n times the game's (60/120/240 Hz) t = k/n; otherwise
  t = time since arrival / frame period. Untagged output (2D, screen filters) is shown as is. A
  mesh is blended if at least half its vertices moved less than 128 px; single vertices moving
  more than 512 px are not. A native 3D mesh needs only some of its strips in both frames (half
  of those paired within 1024 px): one coming into view beside the third-person camera has most of
  its strips only in the newer frame, and left unblended it jumped against its neighbours and
  opened a crack along their shared edges for one image (the elevator door of 9/10).
- **Cuts**: a frame that changed scene or camera is shown at once, without in-between images
  (the previous frame's images end at that frame itself), so no image mixes two cameras. The
  frame's scene is taken when its first mesh is drawn (`view_glue.c pc_frame_view`), because
  `bhCheckCut` changes the cut after drawing: stage, room and, unless the third-person camera is
  on, the cut (`ncut`, `camver`), the event/fixed camera mode with its event camera, and first
  person. In addition the eye and view direction of `cam.mtx` are compared (`stream.c view_cut`):
  a move of more than 10 units or a turn of more than 30 degrees in one game frame is a cut (event
  cameras can switch a frame before their number does; the game's own smooth pans between cuts
  stay below 5 units and 6 degrees). The third-person camera is not compared: it turns fast under
  the mouse but never jumps.
- **Effects** (`effect.c` 3D lists: fire, the lighter flame, muzzle flashes, smoke, sparks, blood)
  are tagged per effect (`pc_effect_tag`): the tag is the effect's `eff[]` slot and that slot's
  life (it starts again when the slot holds another effect or was not drawn in the previous
  frame), and its polygons are numbered. Effect polygons drawn later from the ordering table get
  the tag after their vertices (`Ps2AddOT` writes it to GS register 0x7E, `STREAM_TAG_REG`). So an
  effect pairs only with itself; its position (and size on screen) is blended like a mesh's,
  its animation frame (texture coordinates) and colour are the current frame's. Effects use the
  128 px limit and do not count towards the "most meshes matched" test. 2D effects, lines and
  effects with their own draw function (`ef_fnc`) stay as they are.
- **New meshes move with the camera**: a mesh none of whose instances was drawn in the previous
  frame is placed in the previous frame's view as if it stood still in the world and blended
  from there (`stream.c cam_move`/`nv_camera`, with each frame's `cam.mtx` and projection from
  `view_glue.c take_cam`). Drawn as in the new frame, it stood still on screen while everything
  around it moved, so with a moving camera it jumped against the scene and sank into the wall or
  floor it lies on. Enemies that animate by switching models are such meshes in every switch
  frame: the bugs of 9/17 cycle four models (`en10.c`, `en71.c`) and all switch every third game
  frame. Strips new in a matched mesh move with the camera too when the rest of the mesh did
  (`inst_still`: within a pixel), so a wall's strips coming into view stay joined to it. Effects,
  new strips of moving meshes and meshes whose instances all moved too far to pair stay as in the
  new frame; so does everything when the camera did not change.
- **Menus** ("Smooth menus", `smooth_menus`; `view_glue.c pc_menu_smooth`: on, display-rate
  frames on, and the Firstmovie (title menu), Title, Itemselect (status screen, files, item box),
  Map, Typewriter or Option task running, no movie open): the menus are 2D, which was never tagged,
  so they were shown at 30 FPS. Each 2D polygon or sprite drawn outside a mesh is tagged by its
  look (`pc_menu_tag`, from `native_glue.c pc_native_prim` / `pc_native_prim_raw`): primitive,
  depth, texture and texture coordinates, not position or colour. Tags end in binary 10 (meshes
  are aligned pointers, effects odd). The matcher pairs it with the least-moved polygon of the
  same look in the previous frame (as instances of one mesh, `NATIVE_JUMP`); sprites, lines and
  points (`EV_RAW`) under such a tag become groups too (`RAW_KEY`) and their positions are blended
  in the replay. An untextured piece also carries its colour and which way each corner lies from
  the first (left and right arrows, horizontal and vertical lines differ). A letter of the font
  (`sys->ef_tlist`) carries the letters before it on its line and the steps between them, so a
  changed text never slides letters from one string to another. Looks repeat (both OPTION picture
  bands show the same pictures 368 px apart and scroll round; every row of a list says "On"), so
  two more rules: a pair further apart than `MENU_JUMP` (336 px; the furthest a menu moves a
  piece in a game frame is 320 px, the status screen's panels) is two pieces, and when two
  candidates are about as near (within `AMBIG`, 4 px: a list scrolled a row) the one that moved
  as the menu piece matched before it did is taken, or none. Colours, texture coordinates and new
  or changed parts are the current frame's, so a cursor highlight or a new text shows at once.
  Executed frames are unchanged (the tags only add stream events).
- **Frame blend**: `PS2_swap` mixes each new frame 75% over the previous display image (a
  motion trail). Replays start from the display image as the frame found it, so the trail stays
  correct. With frame blend off the swap copies the frame instead (as during movies).
- **Replay targets**: a replay draws into a second set of render targets swapped in for its
  duration, so the executed frame's targets are never drawn over. Only the parts a frame reads
  before overwriting are copied in. The display target's start image is the previous final
  image (the two swap roles instead of being copied).
- **Static strips**: a matched native strip whose blended fields are bitwise identical in both
  frames is drawn unblended from a per-frame vertex buffer instead of being clipped, converted and
  uploaded again.
- Replays load no CLUT, skip unchanged batch state, set uniforms only when they change, and
  same-value register writes do not end a batch (except inside a two-pass alpha-test batch).

### Presentation and frame rate limit

With vsync on but a swap that returns at once (offscreen drivers, some compositors), the render
loop paces itself to the refresh: it measures the swap duration (averaged over 8 presents; below
1/8 of a refresh means the swap does not wait) and then presents on a fixed grid one refresh
apart. An in-between replay is skipped when the next game frame is already queued, and when the
GPU is still two executed frames or images behind most of a refresh later (`gs_gpu_ready`, a
fence after each): replays queued up behind each other would hold up the next game frame's execution,
and with one frame in flight the game thread would wait for it and drop below 30 updates a second
(Alexia's burning hall at 3x and 32:9 on an integrated GPU: 27.6 updates a second, frames up to
330 ms). Under GPU overload the in-between images thin out instead; the game frames are executed
and shown as before. The frame
rate limit (`fps_limit`: 30/60/90/120/144) caps presents per second below the display refresh
(`render.c present_hz`); the game itself stays at 30.

The game reads the pad at the start of its frame and draws the frame at once, but
`ps2_NaSystem.c Ps2SwapDBuff` hands it over (`PS2_swap`, `sceGsSwapDBuffDc` -> `stream_submit`)
only after waiting for the second VBLANK since the last swap, as the PS2 did: a picture appeared a
whole game frame after the pad read that made it. With "Smooth menus" in a menu
(`pc_menu_smooth`) the hand-over comes first and the wait after it; the game's pace and the order
of its work are the same, only the frame reaches the render thread about 30 ms sooner. Measured
headless in the inventory (press to the first image showing it, 40 presses at random times):
60 Hz 67 -> 34 ms, 120 Hz 60 -> 26 ms on average. Gameplay keeps the original order.

Picture calibration (`brightness`, `contrast`, `gamma`) is applied in the present shader
(`gs_gl.c draw_display`, uniform `u_pic`), so executed frames, in-between images and movies get it
and the overlay drawn afterwards does not. At the neutral values the shader skips it and the
output is bit-identical; `gs_picture` is the same formula on the CPU (F1's test pattern).

`glGetError` is read only every 64th executed frame (reading it after every flush made the
render thread wait for the driver's GL thread).

## VU emulation

VU microcode is not rewritten by hand: the decomp's `vsm/*.vsm` listings are converted to
decoded tables (`pc/tools/vsm2inc.py`). The execution model handles FMAC latency, delayed
MAC/clip flags, Q latency and branch/[E] delay slots, because the original programs are
hand-scheduled around them. The programs are compiled statically: `vu_step()` (one instruction
pair) is inlined into a switch with one case per micro address (`vu_static.inc`), so the C
compiler folds every decoded field into straight-line code. Both paths share `vu_step()`.

Inline EE/VU0 assembly in the decomp is translated to C by `pc/tools/asm2c.py`.

The VU0 time is almost all `VU0_CALCPOINT` (one vertex of a chunk model per call, 97-99% of
the microcode time in every scene) and the macro-mode ops of the translated asm (skinning,
matrices, `Ps2AddPrim3DMod`). These run as native code with the same operations in the same
order, the same clamping (`vu_clampf`: Inf/NaN -> +-FLT_MAX, denormals -> +-0), flags and
resulting VU state:

- `VU0_CALCPOINT` (`vu.c calcpoint_fast`): every light setup, directional and point lights. The
  program has no loops and its stalls only delay, so its timing is resolved statically: which
  DIV/RSQRT result each `MULq` reads (and Q at the end), which FMAC's MAC flags each `FMAND`
  reads, the CLIP flags at `FCGET`. Work whose result the program overwrites is skipped.
- Whole vertex runs (`vu0_calcpoint_n`, called by `njCnkCvVnPs2` through a `TARGET_PC` hook in
  the decomp): four vertices per vector, one per lane, with the same operations per vertex;
  only the last vertex runs through `calcpoint_fast`, which leaves the VU state of the
  per-vertex calls (vi11 and the clip flags are folded over the run). On x86 this kernel is
  also compiled for AVX and chosen at run time (`RECVX_VU_AVX=0`: SSE2).
- Macro mode: `vu_upper` (`vu0.h`) is inline; at the asm2c call sites all its arguments are
  constants, so each COP2 op folds to a few vector instructions. The strip clip test of
  `njCnkCsUvh/Uvn` (`vu_strip_clip`) is the same ops in one loop.
- The four-lane layer (`vu_simd.h`) is SSE2, or plain C with the same results
  (`RECVX_VU_PORTABLE`, any non-x86 build).

The other VU0 programs (`CLIP_VIEW_VOLUME_ALL`, `CALC_COLOR`, `CALCPOINT_INIT/END`, ...) take
about 0.1 ms a frame and stay on the static path, as do the VU1 programs (unused while the
native 3D paths are on).

Exactness: `RECVX_VU_CHECK=1` runs every microprogram call and every CALCPOINT run both ways
and compares the whole VU state (and the run's outputs); `RECVX_VU_INTERP=1` runs all
microprograms (CALCPOINT runs too) through the interpreter. `pc/tests/test_vu.c` (ctest `vu`,
`vu_portable`) compares CALCPOINT, the runs and every macro op (op, field mask, broadcast,
overlapping registers) against the interpreter on random states with overflow, denormal,
zero, -0, Inf and NaN inputs, in SSE2, AVX and plain C. With two NaN operands of ADD or MUL,
which NaN (and so which clamped sign) survives depends on the compiler's operand order, in the
interpreter too; the PS2 has no NaN, and the tests use negative NaNs.

### Cost per frame

Game-thread time per game frame (`--profile`: logic + VU0 + VU1 + GIF recording, mean of game
frames 400-1500 of a headless run; the two builds alternately on an otherwise idle Ryzen 7
9800X3D, Linux, 32-bit build; run to run within about 20%). Before: the build without the
native VU0 paths (CALCPOINT native only without point lights).

| Scene | Before | After |
|---|---|---|
| `RM_0000`, new game, cutscene skipped | 6.0 ms (VU0 4.9) | 1.1 ms |
| `RM_0000`, the opening cutscene | 2.7 ms (VU0 1.9) | 0.6 ms |
| `RM_0060`, walking and shooting zombies (as `layout_check.sh`) | 4.5 ms (VU0 3.2) | 1.0 ms |
| `RM_0060`, standing while a zombie comes | 7.6 ms (VU0 5.9) | 1.3 ms |
| `RM_0030` (outdoors, rain) | 5.8 ms (VU0 4.3) | 1.1 ms |
| Title menu | 0.06 ms (no VU0) | 0.06 ms |
| RE4 village, whole map (`RM_1310` mod, ~200k vertices a frame) | 23.3 ms | 9.6 ms |

Retail rooms have point lights, so CALCPOINT ran through the static interpreter (~0.8 us a
vertex); every macro op was a call with register copies. The village has none: its CALCPOINT
was already native, at ~75 ns a vertex, now ~16 ns. The largest game-thread costs now: CALCPOINT
runs, `pc_native_strip`, `vu_strip_clip`, the other VU0 programs (~0.1 ms) and, in the
village, collision. The render thread (GS emulation, replays: `r_exec`, `r_interp`) is separate:
1.5 ms a game frame in a retail room, 7.7 ms in the village.

## Audio and movies

- **ADX** (`audio/adx.c`, `audio/audio.c`): CRI's ADXF file system (AFS partitions and plain
  files read from the disc image) and the ADXT stream player with the original status and time
  behaviour (the game waits on `ADXT_STAT_PLAYEND` for voice lines). Each stream is an
  `SDL_AudioStream` decoded on demand on SDL's audio thread.
- **Sound effects and MIDI** (`audio/snd_iop.c`, `audio/snd_synth.c`): Capcom's TSNDDRV IOP
  driver is emulated at the SIF RPC level. The TQ request layer follows the retail
  `TSNDDRV.IRX`; HD/BD banks are played on a model of Sony's hardware synthesizer on SPU2 voices,
  with SPU2 reverb presets read from the disc. SQ MIDI sequences (ambient, door, event sounds)
  run on a sequencer with the loop conventions of `MODMIDI.IRX`, on the EE's 10 MIDI ports
  (ports 0 and 1 carry the room's background ambience; the system sounds - typewriter, item box,
  computers, doors - rotate over ports 2, 8 and 9). MIDI bank 1 is the door's (`DOOR_xxx.SPQ`,
  loaded only with the door animation), 2 the common system sequences, 3 the room's ambience.
  - **Timing**: on the PS2 the driver runs on the IOP beside the game; the EE sends one command
    batch per frame (`sdSysServer`) and the IOP's 240 Hz timer ticks the TQ slots and the MIDI
    ports in between. Here the tick runs in the SDL audio callback, every 200 output samples, and
    the RPC handler queues the sound commands in order, stamped with the time they were sent. A
    mixed buffer stands for the time since the previous one, so each command runs at its share of
    the buffer: commands keep their spacing (one game frame) and the ticks fall between them, as
    on the PS2. Bank data commands (HD/BD/SQ uploads) run at once on the game thread, after what
    was queued before them, since the EE reuses the staging buffer and polls the checksum. The
    status block (`se_info`, `midi_info`) is filled from the driver state when the EE reads it
    (`sceSifGetOtherData`). Without an audio device the commands run at once and the ticks follow
    the vblank count.
  - Why: the computers print a letter per frame, each a system MIDI sound that the next letter's
    `sdMidiPlay` stops (`SdrBgmStop` of the previous port). When the ticks ran only at the next
    RPC, a sequence's first note was keyed on and stopped in the same instant and never sounded;
    short notes of other sequences were lost the same way.
- **Movies** (`movie/`, `pc/glue/movie_glue.c`): the disc's FMVs are PSS files (MPEG-2 program
  stream: MPEG-2 video, 320 pixels wide, progressive 29.97 fps, and 16-bit PCM in private
  stream 1), decoded by our own demuxer and MPEG-2 decoder (`pss.c`, `mpeg2.c`). The game's own
  movie code (Sofdec wrapper `mwwrap.c`, `ps2_sfd_mw.c`) runs unchanged: status PREP -> PLAYING
  -> PLAYEND, skip, pause, fades.
  - A **movie thread** (`player.c`) reads the file through its own handle, keeps half a second
    of sound queued on the output device ahead of what plays, and decodes into a ring of 8 YUV
    frames. Neither the game thread nor the render thread decodes.
  - The **game thread** (`readMpeg`) picks the frame for each game frame: one per two vblanks
    (the game runs at 29.97 Hz during movies, the movies' own rate), so the cadence is even; when
    the sound's position (samples handed to the device minus those still queued) is a whole
    frame or more away, the frame number follows it. The movie ends with its last video frame,
    as on the PS2.
  - **Sound level**: the game sets the SPU2's input volume (`changeInputVolume`, 0..0x7FFF
    full), the stream's gain; a skip with a fade (the start-up intro) lowers it to 0, and each
    movie starts at full as the PS2's `audioDecResume` does. The room's sound is the game's:
    `PlayStartMovieEx` stops the BGM and voice streams, and room scripts that want the background
    SE (MIDI) gone stop it themselves before the movie (`BGSeOff`); the graveyard's rain plays on
    under its movie, as on the PS2.
  - **Ending skip**: room 9/36's last event plays the ending (`MV_021`) and the credits (`MV_015`,
    a 4:3 movie), then `RankingCall`; both have no cancel bit in `MovieTypeDef`. `sdfunc.c
    PlayStartMovieEx` gives them cancel with a fade (the intro's), and a press counted by
    `pc_intro_pressed` since the movie began (not the pad's edge) is their cancel button. The fade
    starts from the picture (the room's black screen fade behind it is cleared first), and the play
    time, which counts on through these movies, gets the vblanks the movie had left
    (`pc_movie_vblanks_left`), so the results are those of the movie watched to its end.
  - The game's sprite (`vbrank_draw`, as on the PS2) draws the frame, so size, position and
    the screen fades drawn over it are the game's. Before it, `stream_movie` puts the frame
    number into the GS stream; the **render thread** (`gs_gl.c gs_movie_bind`) uploads the
    three planes once per new frame and converts them on the GPU (BT.601 video range, MPEG-2
    chroma siting, interpolated chroma) into the texture the sprite samples. The PS2 path's
    upload of the IPU's macroblock output into GS memory (`setImageTag`) is gone.
  - The streams have no letterbox bars of their own: the game draws them 640 x 320 (2:1) at
    Y 64 (`sdfunc.c MovieDef`), the 4:3 credits 640 x 448. **Movie zoom** (`movie_zoom`)
    scales that sprite evenly about the screen centre until it reaches the widened view's width
    or the 480 displayed lines, whichever comes first, so only black leaves the screen; at 4:3
    the factor is 1 and the sprite is the game's.
  - Movie frames have no tagged 3D, so the in-between path never blends or replays them: each
    display refresh shows the latest executed frame.

## Input

## Settings

**Adding a setting**: add the variable's ini key to `keys[]` in `config.c` and one entry to
`g_settings[]`, for example

```c
{ SP_GAMEPLAY, "Enemy difficulty", V(g_config.enemy_level), 0, 2, 1, levels,
  .help = "...", .locked = in_session },
```

## Save system

Two save systems, chosen by `save_system` (F1 / ENHANCED SETTINGS GAMEPLAY "Save system"):

- **Native files** (0, default): `<save dir>/saves/NNN_<label>.sav`, one file per save, no limit
  (`pc/src/host/savestore.c`, tested by `pc/tests/test_savestore.c`).
- **Memory card** (1): the PS2 card as before, `pc/src/ps2/mc.c` (libmc on a folder,
  `<save dir>/card0`, 15 save slots plus the system file).

**File format.** A 64-byte header followed by the game's save exactly as the card holds it
(`SAVEFILE`, 0x838 bytes: `sys->version .. save_end` and the game's byte-sum checksum), so a native
save and a card save convert into each other by adding or dropping the header. Header
(little-endian): `"RECVXSAV"`, version 1, data size 0x838, `"SLUS-20184"`, a reserved word, the
time written (64-bit seconds since 1970), play time, save count and difficulty (32-bit each),
character, stage and room (bytes), padding and the header's byte sum. A file counts as a save
only if the header, its sum, the size, the game's checksum and the header's copies of the data's
fields all agree; anything else in the folder is skipped (and logged). The **number** `NNN` is the
save's identity: three digits, more past 999 (`1000_...`), a new save takes the highest number in
the folder plus one (invalid files included, so nothing is ever overwritten by a new save). The
**label** (`Claire_Prison-front`: character and the save menu's room name, made safe for any file
system, UTF-8 kept, at most 64 bytes) is for people and never parsed. Writes go through
`pc_save_file` (temporary file, flush, rename); overwriting a save in another room writes the new
name first and then removes the old file. The system file (`CONFIGFILE`: options, clear flags, best
times; the card's `BASLUS-20184`) is `saves/system.dat`, byte for byte.

**The game's screens.** The typewriter's save screen and LOAD GAME (`ps2_SaveScreen.c`,
`ps2_LoadScreen.c`) keep their states, drawing and messages; `pc/glue/save_glue.c` changes only
where the rows come from and who does the I/O:

| Hook (decomp, `TARGET_PC`) | Native files |
|---|---|
| `ExecuteSaveScreen` / `ExecuteLoadScreen`: `ExecuteMemoryCard` | `pc_saves_native(state)` takes the setting while the screen is in its first state; then `pc_save_card` replaces `ExecuteMemoryCard`: no card polling, and a read or write the screen queued on `McState` (by `ReadMemoryCard` / `WriteMemoryCard` from `mcReadStartSaveFile` / `mcWriteStartSaveFile`, which keep the game's own save logic: save count, ink ribbon, checksum) is done on the chosen row's file, ending in the card's end states (9 read, 10 written) or its error codes |
| `ExecuteState*AwarenessCard` | `pc_save_list` fills the file window (`SELECTFILEWINDOW.pRecordTop`, `lRecordMax`) with the saves in number order, plus an empty row with the next number when saving; straight to the list (no card check, card selection or "checking memory card") |
| `ExecuteState*SelectFile` | `pc_save_move`: up/down through any number of rows, wrapping as the card's 15 do; cancel leaves the screen instead of going back to the card selection |
| `ExecuteState*Save` / `*Load` | the row index can be past the card's 15 file names: file name 0 is passed and `pc_save_card` uses the row |
| `mcDispFileNumber` | numbers past 99 are drawn narrower, ending where two digits end |
| `ExecuteSysLoadScreen` / `ExecuteSysSaveScreen` | `pc_sysload_native` / `pc_syssave_native`: `system.dat` read at boot, merged (`SysSaveHikaku`) and written by OPTION and the ranking, in one frame without the card screens |

**Sounds**: the native screens keep the card screens' own sound calls (cursor, confirm, cancel,
the typed-in row's keys and bell after a save); the card-only steps they skip (slot choice, card
check, the system file's "save?" question) have no screen and play nothing, as no button is
pressed there.

With the memory card these hooks call the original code; `pc/tools/layout_check.sh` with
`INI="save_system = 1"` and a scripted card save and load give byte-identical frames to the build
before the native system.

**Rows and cursor.** Saving: the cursor starts on the new row (last); loading: on the save written
last. An empty row looks like the card's empty slot ("NO_DATA"); loading with no saves shows one.

## Fidelity notes

- Game logic is compiled unchanged. Where the decomp relied on MWCC behaviour standard C does
  not have, the C was rewritten under `TARGET_PC` to the evidently intended semantics:
  `*mkaP->key++` read several times in one expression (`en03.c`, `en23.c`) becomes `key[0]`,
  `key[1]`, `key[2]`; implicit returns of asm functions through `v0`/`$f0` (`njSin`, `njCos`,
  `Ps2BitCount`, ...); functions without a `return` on some path were checked against the retail
  code and fixed where the value is used ([dev/missing-returns.txt](dev/missing-returns.txt)).
- PS2 FPU/VU arithmetic is not IEEE (no denormals, Inf or NaN; truncating rounding). Results
  are clamped like the VU (`vu_clampf`) and the build uses SSE single precision without FMA
  contraction, but bit-exactness with the EE FPU is not guaranteed.
- Uninitialized locals are zero (`-ftrivial-auto-var-init=zero`), a deterministic stand-in for
  MWCC's stale registers ([dev/uninitialized-locals.txt](dev/uninitialized-locals.txt)).
- MWCC compiles a signed division by a power of two as an arithmetic shift (rounding down; C
  rounds toward zero). Where it matters this is written as the shift under `TARGET_PC`:
  `sdfunc.c SE_BANK_NO`, the bank of a sound number (negative numbers are the system MIDI
  sounds: typewriter, item box, computers).
- Functions that were empty in the retail build stay empty (for example `ADXT_Pause`).
- `ps2_sg_sd.c Volume_Control`: retail's MIDI fade up writes the current volume into the fade's
  target and returns a stale register; the PC returns the current volume and leaves the target
  alone (otherwise a MIDI fade up ended silent).
- `player.c bhCPM0_damage`: a damage level above 2 (possible when another enemy's hit takes over
  a grab) is clamped to 2; there are three levels.
- The Windows build gives the game glibc's `rand()` (`pc/src/ps2/stubs.c`): the game assumes
  `RAND_MAX` 0x7fffffff, UCRT's is 0x7fff. Both platforms get the same random sequence.
- Every function the decomp calls is declared on the PC: the original sources call 74
  functions without a prototype (harmless for MWCC on the EE), so `TARGET_PC` blocks include the
  headers (`ps2sdk_shim.h` for the SDK calls). Undeclared, i686 GCC passes a `float` argument as
  a `double` (`njSetScreenProjection`: the Palace monitor of Steve; `bhDispFontEx`) and reads a
  `char` result as the whole register: `bhCheckRoute`/`bhCheckRouteID` return 0xFF as -1 on some
  paths, so `en17.c bhEne17_Brain`'s `!= 255` took the route branch (it only sets Monster Steve's
  route target, which nothing reads; `en09`/`en30` mask with 0xFF, `en19` stores into an
  `unsigned char`). Enforced by the build (CONTRIBUTING.md).
- Translated from the retail ELF instead of the decomp's asm blocks (hand-written asm in the
  original): `bhObj005` (`objitm.c`), `njMirror`, `_ClipInter`, `ScissorTriangle`, `KazuSet`.
- **Fast loading** (`fast_loading`, default on): a room load (`bhSysCallMonitor` mode 4) sets one
  texture list per frame and the sound monitor (`bhSysCallSndMonitor`) uploads each sound pack in
  48 KB packets, one per VBLANK, with a checksum round trip per bank: the pace of the PS2's SIF DMA
  to the sound chip, about 30-60 frames of an Instant door. With the setting on, the texture lists
  are set in one frame and the sound monitor's steps run in one frame, each pack loaded on the game
  task at once (`sdfunc.c pc_load_sound_pack_now`: `LoadSoundPackFile` looped, its read completed
  by the file manager, each download done by `ps2_sg_sd.c pc_snd_download_now` as `sdDrvInit` does,
  re-reading the IOP status after each step so the checksum compares the new bank's sum). A VBLANK
  delivered inside (the disc read polls interrupts) leaves the downloads and the file manager to it.
  The room's effects run in every frame of the load (`ef_flg` bit 0, steps 10-16) and take random
  numbers; with the shorter load they are topped up to 30 frames at step 16, the shortest original
  load, so rain and smoke look settled. The load screen's "Now Loading..." (`bhSysCallMonitor`,
  message 491 while `NowLoadDisp` is set) is not drawn; only the text, the load's steps are
  unchanged. Not in the attract demo (it replays recorded input from
  `srand(1)` at the load). Off, frames are identical to a build without the setting; on, every load
  is shorter and the frames after it (and the random sequence) differ. The log line `room: RM_xxxx ready N frames after
  the door` (or `the game start`) gives the length of each load.

## Determinism

The same build run twice with the same input, and two builds of the same code with a different
binary layout, must give byte-identical frames, in vanilla and in enhanced mode. Game logic is
stepped per game frame, so wall-clock timing does not enter it. What can break it is retail code
reading memory it never wrote (stale stack, bytes past an array): on the PC such bytes are often
addresses, which change with the layout, with ASLR or with what ran before on the stack.

**Check**: `pc/tools/layout_check.sh BIN_A BIN_B OUT [ISO]` runs both binaries headless with the
same scripted input (new game, teleport to room 0/6 with the handgun, walking, shooting, idling
while zombies change motions) in vanilla and enhanced mode (in-between images at
`RECVX_INTERP_T=0.5`), dumps every 10th frame up to 1800 and compares them byte for byte; exit
status 1 on any difference. For a second layout configure another build tree with
`-DRECVX_LAYOUT_PAD=20500` (unused bytes before all code, data and bss). Run to run: pass the same
binary twice. `INI="key = value"` adds lines to the runs' `recvx.ini` (e.g. `save_system = 1`).
It needs the disc image and a GPU, so it is not a ctest.

Retail bugs fixed because they made frames depend on the layout:

| Where | What retail reads | Fix |
|---|---|---|
| `Motion.c` (`bhSetMotion`, `bhGetObjMotion`) | a motion change at `bhGetFrameNum(...) * 65536` starts one past the last key, so the blend reads one key past each array, and after the last array the motion's table of key pointers (heap addresses) | an array whose past-the-end read would hit the pointer table is swapped for the duration of the read for a copy ending in its first key; every other read stays as retail |
| `ps2_NaGraphics3D.c Ps2SetPlane` | copies a pointer parameter plus 12 stale stack bytes as the plane's point; the near-plane cone of `njDrawLine3D` (particle lines, search light, laser sight) went through stack addresses read as floats | the point is the camera point (0, 0, -dist) the planes are meant to pass through |

**Real time by design** (as on the PS2, which runs these off the display and sound hardware):
movies play by vblank count, pulled to the sound's position (`RECVX_SKIP_MOVIES` counts game
frames instead, so test runs repeat), ADX streams report their position from the audio stream, the play time counts vblanks,
and an in-between image's t follows the clock unless `RECVX_INTERP_T` fixes it. A run that waits
on these can shift by frames. The sound driver ticks with the audio output, so the sound status
the EE reads (which sounds still play) follows it too: from run to run a sound trace can differ in
the channel an enemy sound gets and in how many volume/pan updates are sent; the frames do not.

Not covered: other stale or out-of-range reads that the scenarios do not reach. `rand()` is only
called by the game thread.

## Profiling and logging

- `--log[=file]` copies all output (the game's own diagnostics and the port's messages) to a log
  file with a timestamp per line; a crash appends a backtrace (Windows: exception address and
  frame chain).
- `--profile[=file]` writes one CSV line per game frame: total frame time and exclusive time in
  game logic, VU0, VU1, GS emulation + GL submission, interpolation, presentation and idle wait.
  F1 -> Performance shows the same live. The native `CALCPOINT` (single calls and runs) is not
  timed separately and counts as game logic; VU0 is the other microprograms.
- `--profile-samples=file` (Linux, same as `RECVX_PROFILE=file`) samples each thread at 1 kHz of
  its own CPU time; `pc/tools/profile.py build/pc/recvx file [top] [r|g]` summarises by function,
  optionally for the render (`r`) or game (`g`) thread only. `file.maps` lets it name libraries.
- Render-thread wall time in `r_exec`/`r_interp` with low CPU use is time spent waiting for the GL
  driver or GPU.
- `room: RM_xxxx texture table has N list(s) more/fewer than the room reads`: a mod room whose
  texture lists do not match what `bhSetRoom` takes in table order (every later effect would use
  its neighbour's atlas). `pc/tools/assets.py dependencies` applies the same rule.

## Environment variables

### General

### Scripted input

| Variable | Effect |
|---|---|
| `RECVX_INPUT="frame:buttons,..."` | scripted pad input by presented frame, e.g. `200:start,205:-` |
| `RECVX_KEYS="f:key+key,f:-,f:@x:y"` | hold keyboard keys (SDL names, `_` for a space) from presented frame f, sent as key events; `@x:y` left-clicks at a window pixel, `@x:y:r` right-clicks |
| `RECVX_VPAD="f:input+input,..."`, `RECVX_VPAD_TYPE=xbox\|ps5\|generic` | a virtual SDL gamepad holding inputs (ini spelling) from frame f; `axis=value` holds an axis at -1..1 (`leftx=0.5+lefty=-0.5`: the left stick half up-right) |
| `RECVX_MENU_MOUSE="f:x:y[:act],..."` | scripted mouse in menus (window pixels; act `l`/`r` click, `L`/`U` left down/up, `w`/`s` wheel) |
| `RECVX_TPS_MOUSE="frame:dx:dy,..."` | scripted third-person mouse motion |
| `RECVX_UNFOCUS="f:1,f:0"` | hide the window at frame f (focus loss), `0` shows it again |

### Game state

### Rendering

| Variable | Effect |
|---|---|
| `RECVX_NATIVE3D=0/1`, `RECVX_NATIVE_TEX=0/1` | native 3D / native textures off/on |
| `RECVX_NATIVE_RTEX=0` | `StoreRenderTex` through the emulated readback instead of the GPU capture |
| `RECVX_NATIVE_DEBUG=1` | log the first native strips' vertices |
| `RECVX_NO_GUARD=1` | enhanced mode without the widened clip volume (diagnosis) |
| `RECVX_DBG_NOTEX=1` | draw everything untextured |
| `RECVX_DUMP_FRAMES=dir` | every 30th frame (and its in-between predecessor) and all render targets as BMP, with frame statistics |
| `RECVX_DUMP_EVERY=n`, `RECVX_DUMP_FROM=n`, `RECVX_DUMP_OVERLAY=1` | with `RECVX_DUMP_FRAMES`: also every n-th frame; nothing before frame n; also the window image with the overlay |
| `RECVX_DUMP_TEX=dir` | dump decoded textures |
| `RECVX_HD_TEXTURES=dir`, `RECVX_HD_CACHE_MB=n` | HD texture pack folder (instead of `mods/textures`), memory budget of loaded replacements (default 1024) |
| `RECVX_TRACE_HD=1`, `RECVX_HD_DUMP=dir` | log every HD texture lookup and check; write each looked-up original as PNG under its PCSX2 name |
| `RECVX_INTERP_T=t` | the first refresh after each game frame shows the in-between image at t (comparable dumps) |
| `RECVX_INTERP_STATS=1` | per-frame interpolation matching statistics |
| `RECVX_REPLAY_CHECK=1` (or `=n`), `RECVX_REPLAY_CHECK_DIR=dir` | replay each interpolated frame at t = 1, which must reproduce it; log and save differing frames |
| `RECVX_REPLAY_RTSTART=0` | in-between images start from the executed frame's display image (old behaviour) |
| `RECVX_DISPLAY_HZ=n` | assume an n Hz display (e.g. 1000 with vsync off: present as fast as possible) |
| `RECVX_TRACE_PRESENT=file` | one line per present: frame, t, start time, swap duration |
| `RECVX_MOVIE_TRACE=file` | one line per game frame of a movie: time, vblanks, frame shown, game-thread time, sound position and queue, sound minus picture (ms); at the end decode times and frames shown before they were decoded; when the movie opens, starts and ends a `#` line with its sound (open, rate, seconds queued, paused, gain, RMS at that gain) and the room's (BGM, background SE, playing MIDI ports) |
| `RECVX_GPU_TIME=1` | every 2 s: frames and in-between images per second, render-thread CPU and GPU time; GL draws, GS-path sprites and other primitives, native triangles per frame and per in-between image |
| `RECVX_TRACE_NTEX=1` | log native texture binds, misses and render-texture captures |
| `RECVX_TRACE_DMA=1` | log GIF DMA chain tags |
| `RECVX_VU_CHECK=1`, `RECVX_VU_INTERP=1` | compare the static and native VU code with the interpreter on every call / use the interpreter |
| `RECVX_VU_AVX=0` | the CALCPOINT run kernel in SSE2 instead of AVX |
| `RECVX_DUMP_CALCPOINT=n`, `RECVX_DUMP_VU1=n` | dump VU0/VU1 state at the n-th call |
| `RECVX_EFF_TRACE=1` | log live effects each frame |
| `RECVX_TPS_TRACE=1/2`, `RECVX_TPS_AIM_AT=f`, `RECVX_TPS_ORBIT=deg`, `RECVX_ATMOS_TRACE=1` | third person: log free aim (`=2`: aim at the locked enemy, `f` along its hit capsule); turn the view about the player; log light and fog ([THIRDPERSON.md](THIRDPERSON.md)) |

### Other traces

| Variable | Effect |
|---|---|
| `RECVX_TRACE_TASKS=1` | game task (`sys->tk_flg`) changes |
| `RECVX_TRACE_SND=1` | sound driver commands, key-ons, dropped requests (bank not loaded, no such program or block), stolen voices and notes released before they sounded (key-ons print from the audio thread) |
| `RECVX_TRACE_MC=1` | memory card commands and results |
| `RECVX_PAD_TRACE=1` | DS2 buttons of the pad report when they change |
| `RECVX_RUMBLE_TRACE=1` | the game's motor values and what goes to the gamepad (`rumble: frame N motors small S large L -> gamepad low X high Y`) when they change; `RECVX_VPAD`'s virtual gamepad has motors and logs what SDL sends them |
| `RECVX_MODERN_TRACE=1` | modern controls: each frame a direction is translated, the direction, cut (and whether the previous cut is held), target and facing angle, the player's place on screen and the camera |
| `RECVX_MENU_TRACE=1` | menu clicks with the menu state |
| `RECVX_MOUSE_TRACE=1` | who has the mouse, on each change: captured (third person), OS cursor shown, a menu screen up, the player in control, overlay, focus |
| `RECVX_ITEM_TRACE=1` | the bag (equipped slot, lighter slot, slots) and the metal detector's security boxes (`sys->itm[192]`, `[224]`) whenever a word changes, with the box screen's cursors, by presented frame; in 0/9 also the detector's floor zone (`sys->flr_idx`), its "metal carried" flag (`rm_flg` bit 2) and whether it went off (`ev_flg` bit 53) |
| `RECVX_PROFILE=file` | sampling profiler (as `--profile-samples`) |

## Tools

| Tool | Purpose |
|---|---|
| `pc/tools/asm2c.py`, `vsm2inc.py` | EE/VU0 inline asm to C; VU microcode listings to decoded tables |
| `pc/tools/layout_check.sh` | determinism check (above) |
| `pc/tools/compare_native.sh`, `compare_frames.py` | compare native and emulated rendering, frame dumps |
| `pc/tools/profile.py` | summarise sampling profiles |
| `pc/tools/decomp_warnings.py` | regenerate the lists in [dev/](dev/missing-returns.txt) |
| `pc/tools/enemies.py` | enemy census over the room files ([ENEMIES.md](ENEMIES.md)) |
| room and map tools | [modding/README.md](modding/README.md) |
