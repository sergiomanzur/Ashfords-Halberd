/* Exact-build full engine snapshots. No native room initialization is replayed. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include "../src/host/room_snapshot.h"
#include "../src/host/snapshot_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int busy;
static int hash_state;
static unsigned char build_hash[32];
void pad_snapshot_reset(void);
int room_snapshot_busy(void) { return __atomic_load_n(&busy, __ATOMIC_ACQUIRE); }
int room_snapshot_available(void)
{
    /* These developer-only environments contain process-owned cursor strings in
       the spawn survey glue. They are not gameplay and cannot be persisted. */
    return snapshot_core_available() && !getenv("RECVX_SPAWN") && !getenv("RECVX_SPAWN_SURVEY");
}
static int build_ready(void)
{
    if (!hash_state) hash_state = snapshot_executable_hash(build_hash) ? 1 : -1;
    return hash_state == 1;
}
static SnapshotBlob* part(SnapshotImage* image, unsigned id)
{
    for (unsigned i = 0; i < image->count; ++i) if (image->parts[i].id == id) return &image->parts[i].blob;
    return NULL;
}
int room_snapshot_capture(const char* path)
{
    SnapshotImage image = {0};
    int ok = SNAP_INVALID;
    if (!room_snapshot_available() || !build_ready() || __atomic_exchange_n(&busy, 1, __ATOMIC_ACQ_REL)) return SNAP_INVALID;
    memcpy(image.build, build_hash, sizeof(build_hash)); image.count = 5;
    for (unsigned i = 0; i < image.count; ++i) image.parts[i].id = i + 1;
    /* Graphics drains the submitted frame and pending draw environment. Audio
       captures semantic voices/DSP, never FILE/SDL pointers. Core includes the
       room heap, actors, event tasks, local flags and mutable game globals. */
    if (snapshot_render_capture(part(&image, SNAP_RENDER)) &&
        snapshot_audio_capture(part(&image, SNAP_AUDIO)) &&
        snapshot_platform_capture(part(&image, SNAP_PLATFORM)) &&
        snapshot_preferences_capture(part(&image, SNAP_GLUE)) &&
        snapshot_core_capture(part(&image, SNAP_CORE)))
        ok = snapshot_store_write(path, &image);
    snapshot_image_free(&image);
    __atomic_store_n(&busy, 0, __ATOMIC_RELEASE);
    return ok;
}
int room_snapshot_restore(const char* path)
{
    SnapshotImage image = {0};
    void* audio = NULL;
    int result;
    if (!room_snapshot_available() || !build_ready() || __atomic_exchange_n(&busy, 1, __ATOMIC_ACQ_REL)) return SNAP_INVALID;
    result = snapshot_store_read(path, build_hash, &image);
    if (result != SNAP_OK) goto done;
    if (image.count != 5 || !snapshot_core_validate(part(&image, SNAP_CORE)) ||
        !snapshot_render_validate(part(&image, SNAP_RENDER)) || !snapshot_audio_validate(part(&image, SNAP_AUDIO)) ||
        !snapshot_platform_validate(part(&image, SNAP_PLATFORM)) || !snapshot_preferences_validate(part(&image, SNAP_GLUE))) {
        result = SNAP_INVALID; goto done;
    }
    /* Stage audio files, streams and banks before committing any engine memory.
       Renderer similarly stages all GPU allocations before replacing its state. */
    audio = snapshot_audio_prepare(part(&image, SNAP_AUDIO));
    if (!audio || !snapshot_render_restore(part(&image, SNAP_RENDER))) { result = SNAP_IO_ERROR; goto done; }
    /* These routines cannot allocate/fail after successful validation. */
    if (!snapshot_platform_restore(part(&image, SNAP_PLATFORM)) || !snapshot_core_restore(part(&image, SNAP_CORE)) ||
        !snapshot_preferences_restore(part(&image, SNAP_GLUE))) {
        result = SNAP_INVALID; goto done;
    }
    snapshot_audio_commit(audio); audio = NULL;
    pad_snapshot_reset();
    result = SNAP_OK;
done:
    if (audio) snapshot_audio_discard(audio);
    snapshot_image_free(&image);
    __atomic_store_n(&busy, 0, __ATOMIC_RELEASE);
    return result;
}
