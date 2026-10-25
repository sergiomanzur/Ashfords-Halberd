/* A rebuilt stream must consume the same queued PCM and continue the same ADX history. */
#include "../src/audio/audio.c"
HostConfig g_config;
void host_log(const char* fmt, ...) { (void)fmt; }
FILE* pc_fopen(const char* p, const char* m) { return fopen(p, m); }
int cdvd_find_file(const char* p, uint32_t* l, uint32_t* n) { (void)p; (void)l; (void)n; return 0; }
int cdvd_read_sectors(uint32_t l, uint32_t n, void* p) { (void)l; (void)n; (void)p; return 0; }
uint64_t ps2_vblank_count(void) { return 0; }
void ps2_poll_interrupts(void) {}
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void) {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"); CHECK(SDL_Init(SDL_INIT_AUDIO)); g_config.music_volume = 100; audio_init(); CHECK(SDL_PauseAudioDevice(dev));
    unsigned char bytes[8192] = {0}; bytes[0] = 0x80; bytes[2] = 7; bytes[3] = 0xfc; bytes[5] = 18; bytes[7] = 1;
    bytes[8] = 0; bytes[9] = 0; bytes[10] = 0xbb; bytes[11] = 0x80; bytes[13] = 1; bytes[16] = 1; bytes[17] = 0xf4;
    for (unsigned i = 2048; i + 18 <= sizeof(bytes); i += 18) { bytes[i + 1] = 9; memset(bytes + i + 2, 0x23, 16); }
    FILE* f = fopen("snapshot-decoder.tmp", "wb"); CHECK(f); CHECK(fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes)); fclose(f);
    strcpy(g_config.iso_path, "snapshot-decoder.tmp"); int id = audio_stream_start_adx(g_config.iso_path, 0, sizeof(bytes), -20); CHECK(id == 0);
    audio_stream_pause(id, 1); feed(&voices[id], voices[id].stream, 1024, 0);
    SnapshotBlob image = {0}; CHECK(decoder_capture(&image)); CHECK(decoder_validate(&image));
    int16_t expected[512], actual[512], expected_next[64], actual_next[64];
    CHECK(SDL_GetAudioStreamData(voices[id].stream, expected, sizeof(expected)) == sizeof(expected));
    CHECK(decode_frames(&voices[id], expected_next) == 32);
    DecoderStage* s = decoder_stage(&image); CHECK(s); decoder_commit(s);
    CHECK(SDL_GetAudioStreamData(voices[id].stream, actual, sizeof(actual)) == sizeof(actual)); CHECK(!memcmp(expected, actual, sizeof(actual)));
    CHECK(decode_frames(&voices[id], actual_next) == 32); CHECK(!memcmp(expected_next, actual_next, sizeof(actual_next)));
    strcpy(g_config.iso_path, "snapshot-decoder-does-not-exist.tmp"); CHECK(!decoder_stage(&image)); CHECK(voices[id].used && voices[id].f);
    free(image.data); audio_stream_stop(id); SDL_Quit(); remove("snapshot-decoder.tmp"); puts("audio decoder snapshot: ok"); return 0;
}
