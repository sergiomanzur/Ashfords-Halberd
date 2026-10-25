#ifndef ASHFORD_ROOM_SNAPSHOT_H
#define ASHFORD_ROOM_SNAPSHOT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SnapshotBlob { unsigned char* data; uint32_t size; } SnapshotBlob;
/* Owned heap blobs. 1 success, 0 error. Caller frees data with free(). */
int snapshot_render_capture(SnapshotBlob* out); /* game thread, synchronous render-thread barrier */
int snapshot_render_validate(const SnapshotBlob* in);
int snapshot_render_restore(const SnapshotBlob* in); /* invalidates queued/interpolated old frames */
int snapshot_audio_capture(SnapshotBlob* out);
int snapshot_audio_validate(const SnapshotBlob* in);
int snapshot_audio_restore(const SnapshotBlob* in);
void* snapshot_audio_prepare(const SnapshotBlob* in);
void snapshot_audio_commit(void* prepared);
void snapshot_audio_discard(void* prepared);
int snapshot_platform_capture(SnapshotBlob* out);
int snapshot_platform_validate(const SnapshotBlob* in);
int snapshot_platform_restore(const SnapshotBlob* in);
int snapshot_preferences_capture(SnapshotBlob* out);
int snapshot_preferences_validate(const SnapshotBlob* in);
int snapshot_preferences_restore(const SnapshotBlob* in);

enum { SNAP_CORE = 1, SNAP_RENDER = 2, SNAP_AUDIO = 3, SNAP_PLATFORM = 4, SNAP_GLUE = 5 };
typedef struct SnapshotPart { uint32_t id; SnapshotBlob blob; } SnapshotPart;
typedef struct SnapshotImage { unsigned char build[32]; uint32_t count; SnapshotPart parts[8]; } SnapshotImage;
enum { SNAP_IO_ERROR = -2, SNAP_INVALID = -1, SNAP_MISSING = 0, SNAP_OK = 1, SNAP_WRONG_BUILD = 2 };
/* Versioned, checksummed, atomic container. Stages everything before restore. */
int snapshot_store_write(const char* path, const SnapshotImage* image);
int snapshot_store_read(const char* path, const unsigned char build[32], SnapshotImage* out);
void snapshot_image_free(SnapshotImage* image);
int snapshot_executable_hash(unsigned char out[32]);

int room_snapshot_capture(const char* path); /* full core + explicit platform sections */
int room_snapshot_restore(const char* path);
int room_snapshot_available(void);
int room_snapshot_busy(void);
#ifdef __cplusplus
}
#endif
#endif
