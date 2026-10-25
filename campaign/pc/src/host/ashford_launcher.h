#pragma once
#include <stddef.h>

/* SDL must be initialized; returns 1 to boot the selected ISO, 0 to exit.
 * The launcher owns its temporary window, renderer and audio device. */
int ashford_launcher(char *iso, size_t capacity);
