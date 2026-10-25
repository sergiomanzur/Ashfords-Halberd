#include "quickstore.h"
#include "fileio.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

enum { QUICK_HEADER_BYTES = 32 };

/* Fixed byte layout, independent of host structure padding and byte order. */
static const unsigned char header_prefix[28] = {
    'A', 'H', 'Q', 'U', 'I', 'C', 'K', '1',
    'S', 'L', 'U', 'S', '-', '2', '0', '1', '8', '4', 0, 0,
    1, 0, 0, 0, 0x38, 0x08, 0, 0
};

static uint32_t read_le32(const unsigned char* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void write_le32(unsigned char* p, uint32_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static uint32_t payload_crc(const unsigned char* data)
{
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (int i = 0; i < SAVE_DATA_BYTES; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xEDB88320) : 0);
    }
    return ~crc;
}

static int payload_valid(const unsigned char* data)
{
    uint32_t sum = 0;
    if (read_le32(data) != 10) return 0;
    for (int i = 0; i < SAVE_DATA_BYTES - 4; ++i) sum += data[i];
    return read_le32(data + SAVE_DATA_BYTES - 4) == sum;
}

static int write_image(FILE* f, void* image)
{
    const size_t bytes = QUICK_HEADER_BYTES + SAVE_DATA_BYTES;
    return fwrite(image, 1, bytes, f) == bytes ? 0 : -1;
}

int quickstore_write(const char* path, const unsigned char data[SAVE_DATA_BYTES])
{
    unsigned char image[QUICK_HEADER_BYTES + SAVE_DATA_BYTES];
    if (!path || !data) return QUICKSTORE_BAD;
    memcpy(image + QUICK_HEADER_BYTES, data, SAVE_DATA_BYTES);
    if (!payload_valid(image + QUICK_HEADER_BYTES)) return QUICKSTORE_BAD;
    memcpy(image, header_prefix, sizeof(header_prefix));
    write_le32(image + 28, payload_crc(image + QUICK_HEADER_BYTES));
    return pc_save_file(path, "wb", write_image, image) == 0 ? QUICKSTORE_OK : QUICKSTORE_IO;
}

int quickstore_read(const char* path, unsigned char data[SAVE_DATA_BYTES])
{
    unsigned char image[QUICK_HEADER_BYTES + SAVE_DATA_BYTES + 1];
    size_t bytes;
    int io_error;
    FILE* f;
    if (!path || !data) return QUICKSTORE_BAD;
    errno = 0;
    f = pc_fopen(path, "rb");
    if (!f) return errno == ENOENT ? QUICKSTORE_MISSING : QUICKSTORE_IO;
    bytes = fread(image, 1, sizeof(image), f);
    io_error = ferror(f);
    if (fclose(f) != 0) io_error = 1;
    if (io_error) return QUICKSTORE_IO;
    if (bytes != QUICK_HEADER_BYTES + SAVE_DATA_BYTES ||
        memcmp(image, header_prefix, sizeof(header_prefix)) ||
        read_le32(image + 28) != payload_crc(image + QUICK_HEADER_BYTES) ||
        !payload_valid(image + QUICK_HEADER_BYTES))
        return QUICKSTORE_BAD;
    memcpy(data, image + QUICK_HEADER_BYTES, SAVE_DATA_BYTES);
    return QUICKSTORE_OK;
}
