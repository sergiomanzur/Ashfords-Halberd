/* Driver/compositor pacing decision, separated for deterministic transition tests. */
#ifndef ASHFORD_PRESENT_PACING_H
#define ASHFORD_PRESENT_PACING_H
#include <stdint.h>
static inline uint64_t present_next_deadline(uint64_t previous, uint64_t started, uint64_t tick)
{
    uint64_t next = previous ? previous + tick : started + tick;
    /* Preserve the grid through small delivery jitter. After a long GPU wait,
       rebase rather than following a late image with a catch-up burst. */
    return next > started && next - started >= tick / 2 ? next : started + tick;
}
static inline int present_swap_paces(int vsync, double swap_average,
                                    uint64_t swap, uint64_t tick, int limited)
{
    return vsync && swap >= tick / 8 && swap_average >= tick / 8 && !limited;
}
#endif
