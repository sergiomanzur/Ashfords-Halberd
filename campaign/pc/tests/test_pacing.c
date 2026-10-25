#include "../src/host/pacing.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
int main(void)
{
    const uint64_t tick = 16666667;
    /* A recently blocking swap cannot justify an unpaced burst once a driver
       or compositor starts returning immediately (display/mode/focus changes). */
    CHECK(!present_swap_paces(1, 15000000, 50000, tick, 0));
    CHECK(!present_swap_paces(1, 15000000, 0, tick, 0));
    CHECK(present_swap_paces(1, 15000000, 14000000, tick, 0));
    CHECK(!present_swap_paces(0, 15000000, 14000000, tick, 0));
    CHECK(!present_swap_paces(1, 15000000, 14000000, tick, 1));
    CHECK(!present_swap_paces(1, 100000, 14000000, tick, 0));
    CHECK(present_next_deadline(100000000, 100000000, tick)==116666667);
    CHECK(present_next_deadline(100000000, 101000000, tick)==116666667);
    /* The fence finishes 14ms after the scheduled render start: retaining the
       old deadline would submit the next image only 2.667ms later. */
    CHECK(present_next_deadline(100000000, 114000000, tick)==130666667);
    CHECK(present_next_deadline(130666667, 130666667, tick)==147333334);
    CHECK(present_next_deadline(100000000, 200000000, tick)==216666667);
    for (int hz = 30; hz <= 360; hz += 30) {
        uint64_t t = 1000000000u / hz;
        CHECK(present_swap_paces(1, t, t / 2, t, 0));
        CHECK(!present_swap_paces(1, t, t / 32, t, 0));
    }
    printf("pacing transitions: %d failures\n", fails);
    return fails != 0;
}
