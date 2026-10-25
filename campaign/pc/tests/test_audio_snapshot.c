/* Real mixer + CRI + synth snapshot roundtrip; compile with production audio sources. */
#include "../src/ps2_platform.h"
#include "../src/audio/audio.h"
#include "../src/audio/snd.h"
#include "../src/host/room_snapshot.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
HostConfig g_config;
void host_log(const char* fmt, ...) { (void)fmt; }
FILE* pc_fopen(const char* path, const char* mode) { return fopen(path, mode); }
int cdvd_find_file(const char* path, uint32_t* lsn, uint32_t* size) { if (strcmp(path, "SAMPLE.AFS")) return 0; *lsn = 0; *size = 16384; return 1; }
int cdvd_read_sectors(uint32_t lsn, uint32_t count, void* out) { FILE* f = fopen(g_config.iso_path, "rb"); if (!f) return 0; fseek(f, lsn * 2048, SEEK_SET); size_t got = fread(out, 2048, count, f); fclose(f); return got == count; }
void ps2_poll_interrupts(void) {}
uint64_t ps2_vblank_count(void) { return 0; }
void* ADXT_Create(int nch, void* work, int size);
void ADXT_Destroy(void* h);
void ADXT_StartAfs(void* h, int pt, int fid);
int ADXT_GetStat(void* h);
void ADXT_GetTime(void* h, int* n, int* scale);
int ADXF_LoadPartitionNw(int pt, const char* name, void* dir, void* info);
void* pc_mod_open(const char* name);
int ADXF_ReadNw32(void* f, int count, void* out);
void ADXF_Close(void* f);
void* iop_ptr(uint32_t addr);
static void be32put(unsigned char* p, uint32_t x) { p[0] = x >> 24; p[1] = x >> 16; p[2] = x >> 8; p[3] = x; }
static int make_disc(const char* path) {
    unsigned char disc[16384] = {0}; memcpy(disc, "AFS", 3); disc[4] = 1; disc[9] = 8; disc[13] = 56;
    unsigned char* h = disc + 2048; h[0] = 0x80; h[2] = 7; h[3] = 0xfc; h[5] = 18; h[7] = 1;
    be32put(h + 8, 48000); be32put(h + 12, 100000); h[16] = 1; h[17] = 0xf4; h[0x12] = 3;
    be32put(h + 0x18, 1); be32put(h + 0x1c, 0); be32put(h + 0x20, 2048); be32put(h + 0x24, 10000);
    for (int k = 4096; k + 18 <= 16384; k += 18) { disc[k + 1] = 2; memset(disc + k + 2, 0x12, 16); }
    FILE* f = fopen(path, "wb"); if (!f) return 0; int ok = fwrite(disc, 1, sizeof(disc), f) == sizeof(disc); fclose(f); return ok;
}
int main(int argc, char** argv) {
    int silent = argc > 1 && (!strcmp(argv[1], "silent") || !strcmp(argv[1], "read_silent"));
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"); CHECK(SDL_Init(silent ? 0 : SDL_INIT_AUDIO));
    g_config.music_volume = 73; g_config.sfx_volume = 61;
    CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "RECVX_MODS", ".", true));
    CHECK(make_disc("snapshot-audio-disc.tmp")); CHECK(make_disc("snapshot-audio-disc-moved.tmp"));
    strcpy(g_config.iso_path, "snapshot-audio-disc.tmp"); audio_init();
    CHECK(ADXF_LoadPartitionNw(0, "SAMPLE.AFS", NULL, NULL) == 0);
    void* h = ADXT_Create(2, NULL, 0); CHECK(h); ADXT_StartAfs(h, 0, 0); CHECK(ADXT_GetStat(h) == 3);
    audio_stream_pause(0, 1);
    unsigned char sectors[4096]; memset(sectors, 'A', 2048); memset(sectors + 2048, 'B', 2048);
    FILE* mod = fopen("snapshot-audio-mod.tmp", "wb"); CHECK(mod); CHECK(fwrite(sectors, 1, sizeof(sectors), mod) == sizeof(sectors)); fclose(mod);
    void* file = pc_mod_open("snapshot-audio-mod.tmp"); CHECK(file); CHECK(ADXF_ReadNw32(file, 1, sectors) == 1 && sectors[0] == 'A');
    memcpy(iop_ptr(1234), "IOP snapshot", 13);
    g_snd_banks.bd[2].data = malloc(4); CHECK(g_snd_banks.bd[2].data); g_snd_banks.bd[2].size = g_snd_banks.bd[2].cap = 4;
    memcpy(g_snd_banks.bd[2].data, "BANK", 4);
    SnapshotBlob b = {0}; CHECK(snapshot_audio_capture(&b)); CHECK(snapshot_audio_validate(&b));
    CHECK(b.size > 2 * 1024 * 1024); /* includes emulated IOP memory */
    SnapshotBlob bad = { b.data, b.size - 1 }; CHECK(!snapshot_audio_validate(&bad)); CHECK(!snapshot_audio_restore(&bad));
    int n, rate, restored; ADXT_GetTime(h, &n, &rate);
    ADXT_Destroy(h); ADXF_Close(file); memset(iop_ptr(1234), 0, 13); memset(g_snd_banks.bd[2].data, 0, 4);
    strcpy(g_config.iso_path, "snapshot-audio-disc-moved.tmp");
    CHECK(snapshot_audio_restore(&b)); CHECK(ADXT_GetStat(h) == 3);
    ADXT_GetTime(h, &restored, &rate); CHECK((silent || restored == n) && rate == 48000);
    CHECK(!memcmp(iop_ptr(1234), "IOP snapshot", 13)); CHECK(!memcmp(g_snd_banks.bd[2].data, "BANK", 4));
    CHECK(ADXF_ReadNw32(file, 1, sectors) == 1 && sectors[0] == 'B');
    if (!silent) { strcpy(g_config.iso_path, "snapshot-audio-missing.tmp"); CHECK(!snapshot_audio_prepare(&b)); CHECK(ADXT_GetStat(h) == 3); strcpy(g_config.iso_path, "snapshot-audio-disc-moved.tmp"); }
    if (argc > 1 && !strcmp(argv[1], "write")) { FILE* f = fopen("snapshot-audio-image.tmp", "wb"); CHECK(f); CHECK(fwrite(b.data, 1, b.size, f) == b.size); fclose(f); }
    if (argc > 1 && (!strcmp(argv[1], "read") || !strcmp(argv[1], "read_silent"))) {
        FILE* f = fopen("snapshot-audio-image.tmp", "rb"); CHECK(f); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
        SnapshotBlob disk = { malloc(size), size }; CHECK(disk.data); CHECK(fread(disk.data, 1, size, f) == (size_t)size); fclose(f);
        ADXT_Destroy(h); CHECK(snapshot_audio_restore(&disk)); CHECK(ADXT_GetStat(h) == 3); CHECK(!memcmp(iop_ptr(1234), "IOP snapshot", 13)); free(disk.data);
    }
    CHECK(g_config.music_volume == 73 && g_config.sfx_volume == 61);
    b.data[0] ^= 1; CHECK(!snapshot_audio_validate(&b)); b.data[0] ^= 1;
    free(b.data); ADXT_Destroy(h); ADXF_Close(file); SDL_Quit();
    remove("snapshot-audio-disc.tmp"); remove("snapshot-audio-disc-moved.tmp"); remove("snapshot-audio-mod.tmp");
    if (argc > 1 && (!strcmp(argv[1], "read") || !strcmp(argv[1], "read_silent"))) remove("snapshot-audio-image.tmp");
    puts("audio snapshot: ok"); return 0;
}
