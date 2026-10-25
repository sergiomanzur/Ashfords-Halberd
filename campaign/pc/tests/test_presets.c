/* Pure preset rules: applying a baseline must never overwrite unrelated choices. */
#include "../src/ps2_platform.h"
#include "../src/host/modern.h"
#include "../src/host/presets.h"
#include <stdio.h>
#include <string.h>

HostConfig g_config;
int g_modern_on;

#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

/* Independently specified user-facing baseline values. Every row is also mutated
   below to prove the getter notices an individual manual change. */
static const struct { size_t offset; int original, enhanced; } fields[] = {
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

static int* field(HostConfig* config, size_t offset)
{
    return (int*)((unsigned char*)config + offset);
}

int main(void)
{
    HostConfig expected, saved;
    for (int profile = 0; profile <= 1; ++profile) {
        /* Nonzero sentinels across the entire object protect paths, bindings,
           camera/backend, audio, progress, cheats, and future unowned settings. */
        memset(&g_config, 0x35, sizeof(g_config));
        memcpy(&expected, &g_config, sizeof(expected));
        for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
            *field(&expected, fields[i].offset) = profile ? fields[i].enhanced : fields[i].original;
        expected.settings_preset = profile;
        g_modern_on = 7;
        preset_set(profile);
        CHECK(memcmp(&g_config, &expected, sizeof(expected)) == 0);
        CHECK(g_modern_on == profile);
        CHECK(preset_get() == profile);

        /* Any controlled edit derives Custom, but restoring the baseline can
           recover its name because the persisted selection remains intact. */
        for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
            int* value = field(&g_config, fields[i].offset);
            ++*value;
            CHECK(preset_get() == 2);
            CHECK(g_config.settings_preset == profile);
            --*value;
            CHECK(preset_get() == profile);
        }
        g_modern_on = !profile;
        CHECK(preset_get() == 2);
        g_modern_on = profile;
        CHECK(preset_get() == profile);

        /* Reapplying a named preset restores its controlled settings. */
        g_config.brightness = 25;
        preset_set(profile);
        CHECK(memcmp(&g_config, &expected, sizeof(expected)) == 0);
        CHECK(preset_get() == profile);

        /* Choosing Custom is nondestructive, including when all values still
           match a baseline; this marker survives a save/load object roundtrip. */
        expected.settings_preset = 2;
        preset_set(2);
        CHECK(memcmp(&g_config, &expected, sizeof(expected)) == 0);
        CHECK(g_modern_on == profile);
        CHECK(preset_get() == 2);
        memcpy(&saved, &g_config, sizeof(saved));
        memset(&g_config, 0, sizeof(g_config));
        memcpy(&g_config, &saved, sizeof(g_config));
        CHECK(preset_get() == 2);

        preset_set(-1);
        preset_set(3);
        CHECK(memcmp(&g_config, &expected, sizeof(expected)) == 0);
        CHECK(g_modern_on == profile);

        /* Persisted named markers are interpreted only if their actual values
           match, even after loading externally edited configuration. */
        g_config.settings_preset = profile;
        CHECK(preset_get() == profile);
        g_config.gamma = 15;
        CHECK(preset_get() == 2);
        g_config.settings_preset = 99;
        CHECK(preset_get() == 2);
    }
    puts("presets: OK");
    return 0;
}
