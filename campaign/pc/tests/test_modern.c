/* Modern controls' direction math (pc/src/input/modern.c): screen direction + camera -> the
   angle to face, the camera kept over a cut while the direction is held, the turn per frame. */
#include "../src/host/modern.h"
#include <math.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define NEAR(a, b) (fabsf(modern_wrap((a) - (b))) < 1.0f)   /* within 1/182 degree */
#define D(a) MODERN_DEG(a)

/* a camera looking along angle `yaw` (as a character facing it): its right axis on the ground */
static void cam(float yaw, float* rx, float* rz)
{
    float a = yaw * (3.14159265f / 32768.0f);
    *rx = cosf(a); *rz = -sinf(a);
}

int main(void)
{
    float rx, rz, x, z;
    ModernHold h = { 0 };
    static const float dirs[8][2] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, -1 }, { -1, 0 }, { -1, 1 } };

    /* camera looking along -z (angle 0): up = away (0), right = -90 degrees (faces +x), down = 180 */
    cam(0, &rx, &rz);
    CHECK(NEAR(modern_target(rx, rz, 0, 1), 0));
    CHECK(NEAR(modern_target(rx, rz, 1, 0), D(-90)));
    CHECK(NEAR(modern_target(rx, rz, -1, 0), D(90)));
    CHECK(NEAR(modern_target(rx, rz, 0, -1), D(180)));
    /* the 8 directions under cameras all round: the camera's angle minus the stick's (clockwise from up) */
    for (int c = -180; c < 180; c += 15) {
        cam(D(c), &rx, &rz);
        for (int i = 0; i < 8; i++) {
            float s = atan2f(dirs[i][0], dirs[i][1]) * (32768.0f / 3.14159265f);
            CHECK(NEAR(modern_target(rx, rz, dirs[i][0], dirs[i][1]), D(c) - s));
        }
    }
    /* length and scale of the axis do not matter (half-pushed stick, unnormalised matrix) */
    cam(D(30), &rx, &rz);
    CHECK(NEAR(modern_target(rx * 3, rz * 3, 0.2f, 0.2f), modern_target(rx, rz, 1, 1)));
    /* a rolled camera: its right axis tilted (y dropped) still gives the ground direction */
    CHECK(NEAR(modern_target(rx * 0.8f, rz * 0.8f, 1, 0), D(30 - 90)));
    /* a degenerate axis (looking straight down the right axis): as the default camera */
    CHECK(NEAR(modern_target(0, 0, 0, 1), 0));

    /* cut hold: the first frame takes the current camera */
    cam(D(10), &rx, &rz);
    modern_hold(&h, 0, 1, &rx, &rz);
    CHECK(h.held && h.cut == 1 && NEAR(modern_target(rx, rz, 0, 1), D(10)));
    /* same cut, camera tracking: follows */
    cam(D(20), &rx, &rz);
    modern_hold(&h, D(5), 1, &rx, &rz);
    CHECK(NEAR(modern_target(rx, rz, 0, 1), D(20)));
    /* cut 2 (camera turned round), stick about the same: the old camera stays */
    cam(D(170), &x, &z);
    rx = x; rz = z;
    modern_hold(&h, D(-20), 2, &rx, &rz);
    CHECK(NEAR(modern_target(rx, rz, 0, 1), D(20)) && h.cut == 1);
    modern_hold(&h, D(30), 2, &rx, &rz);   /* 25 degrees off: still held */
    CHECK(NEAR(modern_target(rx, rz, 0, 1), D(20)));
    /* back into cut 1 while held: its camera, as before */
    cam(D(20), &rx, &rz);
    modern_hold(&h, D(5), 1, &rx, &rz);
    CHECK(NEAR(modern_target(rx, rz, 0, 1), D(20)));
    /* cut 2 again, then the stick turns by more than MODERN_RELEASE: the new camera */
    rx = x; rz = z;
    modern_hold(&h, D(5), 2, &rx, &rz);
    CHECK(h.cut == 1);
    rx = x; rz = z;
    modern_hold(&h, D(5 + 40), 2, &rx, &rz);
    CHECK(h.cut == 2 && NEAR(modern_target(rx, rz, 0, 1), D(170)));
    /* let go (held = 0): the next push takes the current camera at once */
    h.held = 0;
    cam(D(-60), &rx, &rz);
    modern_hold(&h, D(45), 3, &rx, &rz);
    CHECK(h.cut == 3 && NEAR(modern_target(rx, rz, 0, 1), D(-60)));
    /* the stick angle wraps: 175 -> -175 is 10 degrees, not 350 */
    modern_hold(&h, D(175), 3, &rx, &rz);
    cam(D(100), &rx, &rz);
    modern_hold(&h, D(-175), 4, &rx, &rz);
    CHECK(h.cut == 3);

    /* turning: 40% of the way, at most 30 degrees a frame, the short way round */
    CHECK(NEAR(modern_turn(0, D(30)), D(12)));
    CHECK(NEAR(modern_turn(0, D(180)), D(30)) || NEAR(modern_turn(0, D(180)), D(-30)));
    CHECK(NEAR(modern_turn(D(170), D(-170)), D(8)));
    {   /* 180 degrees: within 5 degrees in a third of a second (10 game frames), no overshoot */
        float ay = 0, t = D(179);
        int n;
        for (n = 0; n < 30 && fabsf(modern_wrap(t - ay)) > D(5); n++) {
            float s = modern_turn(ay, t);
            CHECK(fabsf(modern_wrap(t - (ay + s))) <= fabsf(modern_wrap(t - ay)));
            ay += s;
        }
        CHECK(n <= 10);
    }

    /* third person, gamepad: a stick inside its dead zone is nothing; past it, rescaled from the zone's edge */
    x = 0.1f; z = 0.1f;
    CHECK(modern_stick(&x, &z, 0.2f, 1) == 0 && x == 0 && z == 0);
    x = 0; z = 1;
    CHECK(fabsf(modern_stick(&x, &z, 0.2f, 1) - 1) < 1e-5f && fabsf(z - 1) < 1e-5f);
    x = 1; z = 1;   /* a corner beyond the unit circle: length 1, direction kept */
    CHECK(fabsf(modern_stick(&x, &z, 0.2f, 1) - 1) < 1e-5f && fabsf(x - z) < 1e-6f && fabsf(x - 0.70710678f) < 1e-4f);
    x = -0.6f; z = 0;   /* halfway between the zone's edge and full: 0.5 linear, 0.25 with the camera's curve */
    CHECK(fabsf(modern_stick(&x, &z, 0.2f, 1) - 0.5f) < 1e-5f && fabsf(x + 0.5f) < 1e-5f);
    x = -0.6f; z = 0;
    CHECK(fabsf(modern_stick(&x, &z, 0.2f, 2) - 0.25f) < 1e-5f && fabsf(x + 0.25f) < 1e-5f);
    {   /* the aim response: no jump at the zone's edge, rising, fine near the centre (a tenth past the zone: 1% speed) */
        float last = 0;
        for (int i = 0; i <= 100; i++) {
            float a = i / 100.0f, b = 0, l = modern_stick(&a, &b, 0.15f, 2);
            CHECK(l >= last && l - last < 0.03f);
            last = l;
        }
        x = 0.15f + 0.085f; z = 0;
        CHECK(modern_stick(&x, &z, 0.15f, 2) < 0.011f);
    }
    /* stick -> direction under the third-person camera looking along yaw (tps_glue.c passes its right
       axis (cos yaw, -sin yaw), as cam() here): up walks away from the camera, down towards it,
       right to the camera's right; a half-pushed stick (past the dead zone) the same */
    cam(D(40), &rx, &rz);
    CHECK(NEAR(modern_target(rx, rz, 0, 1), D(40)) && NEAR(modern_target(rx, rz, 0, -1), D(220)));
    CHECK(NEAR(modern_target(rx, rz, 0.4f, 0), D(-50)) && NEAR(modern_target(rx, rz, -0.4f, 0), D(130)));
    x = 0.3f; z = -0.3f;
    modern_stick(&x, &z, 0.2f, 1);
    CHECK(NEAR(modern_target(rx, rz, x, z), D(40 - 135)));
    /* a quick turn swings the camera half round: the held stick keeps the camera it was given in */
    h.held = 0;
    modern_hold(&h, D(180), 7, &rx, &rz);                 /* pulled back: walk towards the camera at 40 degrees */
    CHECK(NEAR(modern_target(rx, rz, 0, -1), D(220)));
    cam(D(220), &rx, &rz);                                /* the camera swung behind the turned character */
    modern_hold(&h, D(180), 8, &rx, &rz);
    CHECK(NEAR(modern_target(rx, rz, 0, -1), D(220)));    /* still held back: on the way the character faces */
    cam(D(220), &rx, &rz);
    modern_hold(&h, 0, 8, &rx, &rz);                      /* pushed up instead: the new camera */
    CHECK(NEAR(modern_target(rx, rz, 0, 1), D(220)));

    /* gentle aim assist: outside the cone nothing changes */
    {
        float dy = D(2), dp = D(-1);
        modern_assist(D(10), 0, &dy, &dp);
        CHECK(NEAR(dy, D(2)) && NEAR(dp, D(-1)));
        /* inside: half speed, plus a tenth of the way towards the target while moving towards it */
        dy = D(2); dp = 0;
        modern_assist(D(4), D(1), &dy, &dp);
        CHECK(NEAR(dy, D(1) + D(0.4f)) && NEAR(dp, D(0.1f)));
        /* moving away from it: only half speed (it never holds the aim) */
        dy = D(-2); dp = 0;
        modern_assist(D(4), D(1), &dy, &dp);
        CHECK(NEAR(dy, D(-1)) && NEAR(dp, 0));
        /* a still stick: no pull */
        dy = dp = 0;
        modern_assist(D(4), D(1), &dy, &dp);
        CHECK(dy == 0 && dp == 0);
    }
    printf("modern: ok\n");
    return 0;
}
