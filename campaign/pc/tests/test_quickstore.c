#include "../src/host/quickstore.h"
#include "../src/host/fileio.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); return 1; } } while (0)
static void checksum(unsigned char* d)
{
    unsigned sum = 0;
    for (int i = 0; i < SAVE_DATA_BYTES - 4; ++i) sum += d[i];
    memcpy(d + SAVE_DATA_BYTES - 4, &sum, 4);
}
static int raw(const char* path, const unsigned char* d, int n)
{
    FILE* f = pc_fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(d, 1, n, f) == (size_t)n;
    return fclose(f) == 0 && ok;
}
int main(void)
{
    const char* dir = "test_quickstore_\xC3\x9C" "ber";
    char path[128], bad[128];
    unsigned char data[SAVE_DATA_BYTES] = {10}, out[SAVE_DATA_BYTES], image[SAVE_DATA_BYTES + 33];
    snprintf(path, sizeof(path), "%s/quicksave.ahs", dir);
    snprintf(bad, sizeof(bad), "%s/corrupt.ahs", dir);
    CHECK(SDL_CreateDirectory(dir)); SDL_RemovePath(path); SDL_RemovePath(bad);
    memset(out, 0xDD, sizeof(out));
    CHECK(quickstore_read(path, out) == QUICKSTORE_MISSING && out[0] == 0xDD);
    data[100] = 73; checksum(data);
    CHECK(quickstore_write(path, data) == QUICKSTORE_OK);
    CHECK(quickstore_read(path, out) == QUICKSTORE_OK && !memcmp(out, data, sizeof(data)));
    FILE* f = pc_fopen(path, "rb"); CHECK(f);
    int n = fread(image, 1, sizeof(image), f); fclose(f);
    CHECK(n == SAVE_DATA_BYTES + 32);
    /* A bad write must preserve the previous slot. */
    data[100] ^= 1;
    CHECK(quickstore_write(path, data) == QUICKSTORE_BAD);
    CHECK(quickstore_read(path, out) == QUICKSTORE_OK && out[100] == 73);
    data[100] = 91; checksum(data);
    CHECK(quickstore_write(path, data) == QUICKSTORE_OK);
    CHECK(quickstore_read(path, out) == QUICKSTORE_OK && out[100] == 91);
    /* Every header field, native byte and trailing/truncated data is checked. */
    for (int offset = 0; offset < n; ++offset) {
        image[offset] ^= 1; CHECK(raw(bad, image, n));
        memset(out, 0xDD, sizeof(out));
        CHECK(quickstore_read(bad, out) == QUICKSTORE_BAD && out[0] == 0xDD);
        image[offset] ^= 1;
    }
    CHECK(raw(bad, image, n - 1)); CHECK(quickstore_read(bad, out) == QUICKSTORE_BAD);
    image[n] = 0;
    CHECK(raw(bad, image, n + 1)); CHECK(quickstore_read(bad, out) == QUICKSTORE_BAD);
    CHECK(quickstore_write(dir, data) == QUICKSTORE_IO); /* rename over directory must fail */
    CHECK(quickstore_read(path, out) == QUICKSTORE_OK && out[100] == 91);
    SDL_RemovePath(path); SDL_RemovePath(bad); SDL_RemovePath(dir);
    puts("quickstore: atomic slot, native checksum, full CRC, UTF-8 and corrupt-file isolation verified");
    return 0;
}
