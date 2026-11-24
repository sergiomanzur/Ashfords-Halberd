/* Modern controls for the fixed cameras (pc/glue/modern_glue.c, docs/USER_GUIDE.md "Modern
   controls"): the stick or d-pad points where to walk on screen. The direction math is pure
   (pc/src/input/modern.c, ctest "modern"). Angles in game units (65536 = 360 degrees; a
   character at angle a faces (-sin a, 0, -cos a)). */
#ifndef RECVX_MODERN_H
#define RECVX_MODERN_H
#ifdef __cplusplus
extern "C" {
#endif

extern int g_modern_on;   /* "Fixed camera controls" (ini fixed_cam_controls): 0 classic (tank), 1 modern,
                             MODERN_COMBINED the stick modern and the gamepad's d-pad classic */
#define MODERN_COMBINED 2

#define MODERN_DEG(a) ((a) * (65536.0f / 360.0f))

float modern_wrap(float a);   /* into -32768..32768 */

/* the angle to face for a screen direction (sx right, sy up) under a camera whose right axis
   on the ground is (rx, rz) */
float modern_target(float rx, float rz, float sx, float sy);

/* camera cuts while the direction is held: the camera of the cut the direction was given in
   stays until the direction turns by more than MODERN_RELEASE or is let go */
#define MODERN_RELEASE MODERN_DEG(35.0f)
typedef struct ModernHold { int held, cut; float rx, rz, ref; } ModernHold;
/* stick: the screen direction's angle; cut: the current cut; rx/rz: in the current camera's
   right axis, out the one to use */
void modern_hold(ModernHold* h, float stick, int cut, float* rx, float* rz);

/* one game frame's turn from facing `ay` towards `target`: a third of the way, at most 30
   degrees (180 degrees in about 9 frames, 0.3 s) */
float modern_turn(float ay, float target);

/* Third person with a gamepad, modern (twin-stick) scheme (pc/glue/tps_glue.c, docs/THIRDPERSON.md
   "Gamepad"). A stick (x right, y up, -1..1) past a radial dead zone `dead` (0..1): rescaled so
   the zone's edge is 0 and full deflection 1, its length raised to `curve` (1 linear, 2 finer
   near the centre: the camera and aim response). Returns the new length; 0 inside the zone. */
float modern_stick(float* x, float* y, float dead, float curve);

/* gentle aim assist (the gamepad with "Game auto-aim" on): with a target within
   MODERN_ASSIST_CONE of the crosshair (off_yaw/off_pitch: the target minus the crosshair) the
   stick's turn this frame (dyaw/dpitch) is halved, and while it moves and not away from the
   target the aim drifts a tenth of the way towards it. A still stick does nothing. */
#define MODERN_ASSIST_CONE MODERN_DEG(6.0f)
void modern_assist(float off_yaw, float off_pitch, float* dyaw, float* dpitch);

#ifdef __cplusplus
}
#endif
#endif
