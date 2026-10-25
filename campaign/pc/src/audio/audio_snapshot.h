/* Same-build semantic audio payload helpers. No host handles cross this boundary. */
#pragma once
#include "../host/room_snapshot.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define AUDIO_SNAPSHOT_LIMIT (128u * 1024u * 1024u)
typedef struct { unsigned char* p; uint32_t n, cap; int ok; } AudioWriter;
typedef struct { const unsigned char* p; uint32_t n; int ok; } AudioReader;
static inline void as_put(AudioWriter* w, const void* p, uint32_t n) {
    if (!w->ok || n > AUDIO_SNAPSHOT_LIMIT - w->n) { w->ok = 0; return; }
    if (w->n + n > w->cap) { uint32_t cap = w->n + n; void* q = realloc(w->p, cap); if (!q) { w->ok = 0; return; } w->p = q; w->cap = cap; }
    if (n) memcpy(w->p + w->n, p, n);
    w->n += n;
}
static inline void as_get(AudioReader* r, void* p, uint32_t n) {
    if (!r->ok || n > r->n) { r->ok = 0; return; }
    if (n && p) memcpy(p, r->p, n);
    r->p += n; r->n -= n;
}
static inline int as_finish(AudioWriter* w, SnapshotBlob* out) { if (!w->ok) { free(w->p); return 0; } out->data = w->p; out->size = w->n; return 1; }
#define AUDIO_PART_API(name) \
int name##_capture(SnapshotBlob* out); \
int name##_validate(const SnapshotBlob* in); \
void* name##_stage(const SnapshotBlob* in); \
void name##_discard(void* stage); \
void name##_commit(void* stage)
AUDIO_PART_API(adx_snapshot);
AUDIO_PART_API(snd_snapshot);
AUDIO_PART_API(snd_iop_snapshot);
AUDIO_PART_API(sif_snapshot);
