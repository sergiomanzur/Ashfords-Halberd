#include "presets.h"
#include "../ps2_platform.h"
#include "modern.h"

/* Only presentation and these QOL choices belong to a preset. In particular,
   camera mode, renderer, saving, progress, bindings and audio are independent. */
static const struct { size_t offset; int original, enhanced; } preset_fields[] = {
    { offsetof(HostConfig, aspect), ASPECT_4_3, ASPECT_WIDE },
    { offsetof(HostConfig, scale), 1, 0 },
    { offsetof(HostConfig, interpolate), 0, 1 },
    { offsetof(HostConfig, menu_smooth), 0, 1 },
    { offsetof(HostConfig, frame_blend), 1, -1 },
    { offsetof(HostConfig, screen_filter), 0, 1 },
    { offsetof(HostConfig, cinema_bars), 0, 1 },
    { offsetof(HostConfig, screen_effects), 0, 1 },
    { offsetof(HostConfig, movie_zoom), 0, 0 },
    { offsetof(HostConfig, fog_intensity), 100, 100 },
    { offsetof(HostConfig, brightness), 0, 0 },
    { offsetof(HostConfig, contrast), 100, 100 },
    { offsetof(HostConfig, gamma), 10, 10 },
    { offsetof(HostConfig, saturation), 100, 100 },
    { offsetof(HostConfig, antialias), 0, 0 },
    { offsetof(HostConfig, skip_doors), 0, 2 },
    { offsetof(HostConfig, fast_loading), 0, 1 },
    { offsetof(HostConfig, quick_knife), 0, 1 },
    { offsetof(HostConfig, auto_stairs), 0, 1 }
};

int preset_get(void)
{
    int selected = g_config.settings_preset;
    if (selected != 0 && selected != 1) return 2;
    if (g_modern_on != selected) return 2;
    for (size_t i = 0; i < sizeof(preset_fields) / sizeof(preset_fields[0]); ++i) {
        const int* value = (const int*)((const unsigned char*)&g_config + preset_fields[i].offset);
        int expected = selected ? preset_fields[i].enhanced : preset_fields[i].original;
        if (*value != expected) return 2;
    }
    /* The marker is deliberately not rewritten after a manual change: reverting
       that change recovers the named preset. An explicit Custom stays Custom. */
    return selected;
}

void preset_set(int value)
{
    if (value < 0 || value > 2) return;
    if (value != 2) {
        for (size_t i = 0; i < sizeof(preset_fields) / sizeof(preset_fields[0]); ++i) {
            int* field = (int*)((unsigned char*)&g_config + preset_fields[i].offset);
            *field = value ? preset_fields[i].enhanced : preset_fields[i].original;
        }
        g_modern_on = value;
    }
    g_config.settings_preset = value;
}
