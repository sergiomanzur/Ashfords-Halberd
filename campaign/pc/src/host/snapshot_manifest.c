#include "snapshot_manifest.h"
_Static_assert(sizeof(SnapshotManifest) == SNAPSHOT_MANIFEST_BYTES, "manifest byte layout");
#if defined(__GNUC__)
__attribute__((section(".ahsnap"), used, aligned(4)))
#endif
volatile const SnapshotManifest ah_snapshot_manifest = {
    .magic = {'A','H','S','M','A','P','0','2'}, .version = SNAPSHOT_MANIFEST_VERSION
};
