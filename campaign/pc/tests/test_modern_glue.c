/* Exercise the real fixed-camera input hook with game state, without a renderer. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include "../src/host/modern.h"
#include <stdio.h>
#include <string.h>

CAM_WORK cam;
static float camera_matrix[16];
BH_PWORK ply;
BH_PWORK* plp = &ply;
static SYS_WORK state;
SYS_WORK* sys = &state;
int tps_on;
static float move_x, move_y;
static int analog, available = 1;
static uint64_t frame = 100;
int pad_move_dir(float* x, float* y) { *x = move_x; *y = move_y; return analog; }
int pad_quickturn_held(void) { return 0; }
uint64_t gs_frame_count(void) { return frame; }
int pc_in_play(const BH_PWORK* p, int scope) { (void)p; (void)scope; return available; }
unsigned int modern_pad(unsigned int pad);
void modern_player(void);
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void reset(void)
{
    memset(&ply, 0, sizeof(ply));
    memset(&state, 0, sizeof(state));
    memset(&cam, 0, sizeof(cam));
    memset(camera_matrix, 0, sizeof(camera_matrix));
    cam.mtx = &camera_matrix;
    plp = &ply; ply.mode0 = 1; sys->sp_flg = 0x41;
    ((float*)cam.mtx)[0] = 1;
    available = 1; analog = 0; move_x = move_y = 0;
    modern_pad(0); /* release the previous direction */
    frame += 100;
}

int main(void)
{
    /* Puzzle scripts pause player simulation, retaining its ordinary movement mode.
       Their directional presses must reach the script unchanged in both directions. */
    reset(); sys->sp_flg &= ~1u; move_x = 1;
    CHECK(modern_pad(0x8) == 0x8);
    move_x = -1; CHECK(modern_pad(0x4) == 0x4);
    move_x = 1; analog = 1; CHECK(modern_pad(0x8) == 0x8);

    /* Scripted displacement/interaction owns facing even in the ordinary movement mode. */
    reset(); ply.flg = 0x4000000; move_x = 1;
    CHECK(modern_pad(0x8) == 0x8);
    modern_player(); CHECK(ply.ay == 0);

    /* An interaction can seize control inside the native mode handler, after pad conversion. */
    reset(); move_x = 1; modern_pad(0x8); available = 0;
    modern_player(); CHECK(ply.ay == 0);
    reset(); move_x = 1; modern_pad(0x8); ply.flg = 0x4000000;
    modern_player(); CHECK(ply.ay == 0);

    /* Running remains normal away from a movable object. On push contact,
       auto-run and a held run key must yield to the native walking/push state. */
    reset(); move_y = 1; analog = 1;
    CHECK(modern_pad(0x401) & 0x400);
    ply.psh_ct = 1;
    CHECK((modern_pad(0x401) & 0x401) == 0x1);
    analog = 0; CHECK((modern_pad(0x401) & 0x401) == 0x1);
    ply.psh_ct = 0; CHECK(modern_pad(0x401) & 0x400);

    /* Ordinary movement still follows the camera. */
    reset(); move_x = 1; modern_pad(0x8); modern_player(); CHECK(ply.ay < 0);

    /* A staircase changes cut while steering itself. On returning to ordinary movement,
       held up takes the new camera, including a cut during the ten-frame return window. */
    reset(); move_y = 1; modern_pad(0x1);
    frame++; ply.mode2 = 14; cam.ncut = 1;
    ((float*)cam.mtx)[0] = -1; modern_pad(0x1);
    frame++; ply.mode2 = 0; modern_pad(0x1); modern_player(); CHECK(ply.ay != 0);
    frame++; cam.ncut = 2; ((float*)cam.mtx)[0] = 1; ply.ay = 0;
    modern_pad(0x1); modern_player(); CHECK(ply.ay == 0);

    printf("modern glue: %d failures\n", fails);
    return fails != 0;
}
