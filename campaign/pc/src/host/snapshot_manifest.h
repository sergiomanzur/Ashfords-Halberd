#ifndef ASHFORD_SNAPSHOT_MANIFEST_H
#define ASHFORD_SNAPSHOT_MANIFEST_H
#include <stdint.h>
#define SNAPSHOT_MANIFEST_VERSION 2u
#define SNAPSHOT_MANIFEST_CAPACITY 4096u
#define SNAPSHOT_MANIFEST_BYTES (24u + SNAPSHOT_MANIFEST_CAPACITY * 8u)
typedef struct SnapshotRegion { uint32_t address, size; } SnapshotRegion;
typedef struct SnapshotManifest {
    unsigned char magic[8];
    uint32_t version, count, total_bytes, descriptor_crc;
    SnapshotRegion regions[SNAPSHOT_MANIFEST_CAPACITY];
} SnapshotManifest;
/* A post-link tool fills this fixed-size section. Unpatched count=0 is unavailable.
   Volatile and a separate translation unit prevent folding the initial count. */
extern volatile const SnapshotManifest ah_snapshot_manifest;
#endif
