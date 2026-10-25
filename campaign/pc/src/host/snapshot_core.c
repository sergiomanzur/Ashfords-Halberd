/* Fixed-module snapshots: every destination comes from the current executable's
   post-link manifest. Serialized data never supplies a pointer or destination. */
#include "snapshot_core.h"
#include "snapshot_manifest.h"
#include <stdlib.h>
#include <string.h>

enum { CORE_HEADER_BYTES = 24 };
static const unsigned char core_magic[8] = {'A','H','C','O','R','E','0','2'};
static void write_le32(unsigned char* p, uint32_t v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static uint32_t descriptor_crc(const SnapshotManifest* manifest)
{
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (uint32_t i = 0; i < manifest->count; ++i) {
        unsigned char bytes[8];
        write_le32(bytes, manifest->regions[i].address);
        write_le32(bytes + 4, manifest->regions[i].size);
        for (unsigned j = 0; j < sizeof(bytes); ++j) {
            crc ^= bytes[j];
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xEDB88320) : 0);
        }
    }
    return ~crc;
}
static int current_manifest(SnapshotManifest* manifest)
{
    const volatile unsigned char* source = (const volatile unsigned char*)&ah_snapshot_manifest;
    unsigned char* target = (unsigned char*)manifest;
    uint32_t total = 0, previous_end = UINT32_C(0x30000000);
    /* Read the patched bytes, not compile-time initializer values. */
    for (size_t i = 0; i < sizeof(*manifest); ++i) target[i] = source[i];
    if (memcmp(manifest->magic, "AHSMAP02", 8) ||
        manifest->version != SNAPSHOT_MANIFEST_VERSION || !manifest->count ||
        manifest->count > SNAPSHOT_MANIFEST_CAPACITY)
        return 0;
    for (uint32_t i = 0; i < manifest->count; ++i) {
        const SnapshotRegion* r = &manifest->regions[i];
        if (r->address < previous_end || r->address >= UINT32_C(0x40000000) ||
            !r->size || r->size > UINT32_C(0x40000000) - r->address ||
            r->size > UINT32_C(0x10000000) - total)
            return 0;
        previous_end = r->address + r->size;
        total += r->size;
    }
    return total == manifest->total_bytes && descriptor_crc(manifest) == manifest->descriptor_crc;
}
static void encode_header(unsigned char* out, const SnapshotManifest* manifest)
{
    memcpy(out, core_magic, sizeof(core_magic));
    write_le32(out + 8, manifest->version);
    write_le32(out + 12, manifest->count);
    write_le32(out + 16, manifest->total_bytes);
    write_le32(out + 20, manifest->descriptor_crc);
}
static int valid_blob(const SnapshotBlob* in, const SnapshotManifest* manifest)
{
    unsigned char expected[CORE_HEADER_BYTES];
    if (!in || !in->data || in->size != CORE_HEADER_BYTES + manifest->total_bytes) return 0;
    encode_header(expected, manifest);
    return !memcmp(in->data, expected, sizeof(expected));
}
int snapshot_core_available(void)
{
    SnapshotManifest manifest;
    return current_manifest(&manifest);
}
uint32_t snapshot_core_signature(void)
{
    SnapshotManifest manifest;
    return current_manifest(&manifest) ? manifest.descriptor_crc : 0;
}
int snapshot_core_capture(SnapshotBlob* out)
{
    SnapshotManifest manifest;
    SnapshotBlob staged;
    size_t offset = CORE_HEADER_BYTES;
    if (!out || !current_manifest(&manifest)) return 0;
    staged.size = CORE_HEADER_BYTES + manifest.total_bytes;
    staged.data = malloc(staged.size);
    if (!staged.data) return 0;
    encode_header(staged.data, &manifest);
    for (uint32_t i = 0; i < manifest.count; ++i) {
        const SnapshotRegion* r = &manifest.regions[i];
        memcpy(staged.data + offset, (const void*)(uintptr_t)r->address, r->size);
        offset += r->size;
    }
    *out = staged;
    return 1;
}
int snapshot_core_validate(const SnapshotBlob* in)
{
    SnapshotManifest manifest;
    return current_manifest(&manifest) && valid_blob(in, &manifest);
}
int snapshot_core_restore(const SnapshotBlob* in)
{
    SnapshotManifest manifest;
    size_t offset = CORE_HEADER_BYTES;
    if (!current_manifest(&manifest) || !valid_blob(in, &manifest)) return 0;
    /* Precondition: ps2_memory_init() has completed. In particular, its Windows
       VirtualProtect / Linux mprotect setup makes the game's .rdata writable;
       the original game modifies literals, so these bytes belong in the core.
       All lengths and the entire manifest identity were checked before the
       first write. Gaps, host globals, and the manifest remain untouched. */
    for (uint32_t i = 0; i < manifest.count; ++i) {
        const SnapshotRegion* r = &manifest.regions[i];
        memcpy((void*)(uintptr_t)r->address, in->data + offset, r->size);
        offset += r->size;
    }
    return 1;
}
