#ifndef ASHFORD_SNAPSHOT_CORE_H
#define ASHFORD_SNAPSHOT_CORE_H
#include "room_snapshot.h"
#ifdef __cplusplus
extern "C" {
#endif
int snapshot_core_available(void);
uint32_t snapshot_core_signature(void);
int snapshot_core_capture(SnapshotBlob* out);
int snapshot_core_validate(const SnapshotBlob* in);
int snapshot_core_restore(const SnapshotBlob* in);
#ifdef __cplusplus
}
#endif
#endif
