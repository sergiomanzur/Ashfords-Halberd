/* Exact next-sample continuation of the real DSP, including reconstructed references. */
#include "../src/audio/snd_synth.c"
HostConfig g_config;
SndBanks g_snd_banks;
void host_log(const char* fmt, ...) { (void)fmt; }
int cdvd_find_file(const char* path, uint32_t* lsn, uint32_t* size) { (void)path; (void)lsn; (void)size; return 0; }
int cdvd_read_sectors(uint32_t lsn, uint32_t count, void* out) { (void)lsn; (void)count; (void)out; return 0; }
uint64_t ps2_vblank_count(void) { return 100; }
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
static void next_audio(float out[192]) {
    float fx0[192] = {0}, fx1[192] = {0}; memset(out, 0, 192 * sizeof(float));
    render(out, fx0, fx1, 96); reverb_run(&rev[1], fx1, out, 96);
}
int main(void) {
    CHECK(SDL_Init(0)); snd_synth_init(0);
    SndBlob* b = &g_snd_banks.bd[0]; b->data = malloc(16); CHECK(b->data); b->size = b->cap = 16;
    memset(b->data, 0x35, 16); b->data[0] = 0x12; b->data[1] = 3;
    mport[2].ch[4] = (MidiCh){0, 127, 64, 0x2000};
    voice[0] = (Voice){ .on = 1, .chan = &mport[2].ch[4], .bd = b, .pos28 = 28, .h1 = 1234, .h2 = -432, .frac = 137,
        .base_pitch = 0.8, .vol = 0.7f, .dry = 1, .wet = 1, .core = 1, .phase = 2, .level = 20000, .adsr2 = 0x3f00 };
    rev[1].mode = 1; rev[1].depth = 0.2f; rev[1].pos = REV_RING + 99; rev[1].phase = 1;
    rev[1].reg[2] = 12000; rev[1].reg[3] = 8000; rev[1].reg[30] = rev[1].reg[31] = 22000; rev[1].ring[99] = 532;
    active_bits[0] = 4; mono = 1; user_volume = .6f;
    SnapshotBlob image = {0}; CHECK(snd_snapshot_capture(&image)); CHECK(snd_snapshot_validate(&image));
    float expected[192], actual[192]; next_audio(expected);
    memset(voice, 0, sizeof(voice)); memset(rev, 0, sizeof(rev)); memset(b->data, 0, 16);
    mono = 0; user_volume = .37f;
    void* stage = snd_snapshot_stage(&image); CHECK(stage); snd_snapshot_commit(stage);
    CHECK(voice[0].chan == &mport[2].ch[4] && voice[0].bd == &g_snd_banks.bd[0]); CHECK(active_bits[0] == 4);
    CHECK(mono == 0 && user_volume == .37f); /* current user's preferences survive */
    mono = 1; user_volume = .6f; next_audio(actual); CHECK(!memcmp(expected, actual, sizeof(actual)));
    SynthSnapshot* corrupt = (SynthSnapshot*)image.data; corrupt->chan_id[0] = 999999; CHECK(!snd_snapshot_validate(&image)); CHECK(!snd_snapshot_stage(&image));
    free(image.data); free(g_snd_banks.bd[0].data); SDL_Quit(); puts("audio synth snapshot: ok"); return 0;
}
