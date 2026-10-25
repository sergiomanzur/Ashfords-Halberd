/* Bounded private codec for same-executable renderer snapshots. No process pointers. */
#ifndef RECVX_RENDER_SNAPSHOT_CODEC_H
#define RECVX_RENDER_SNAPSHOT_CODEC_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../host/room_snapshot.h"
#define RENDER_SNAPSHOT_MAX (512u * 1024u * 1024u)
typedef struct { unsigned char *data; uint32_t size, capacity; int bad; } RenderWriter;
typedef struct { const unsigned char *data; uint32_t size, pos; int bad; } RenderReader;
static inline void rs_write(RenderWriter *w, const void *data, uint32_t size)
{
    if (w->bad || size > RENDER_SNAPSHOT_MAX - w->size) { w->bad = 1; return; }
    uint32_t need = w->size + size;
    if (need > w->capacity) {
        uint32_t cap = need > RENDER_SNAPSHOT_MAX / 2 ? RENDER_SNAPSHOT_MAX : need * 2;
        void *p = realloc(w->data, cap);
        if (!p) { w->bad = 1; return; }
        w->data = p; w->capacity = cap;
    }
    if (size) memcpy(w->data + w->size, data, size);
    w->size = need;
}
static inline void rs_u32(RenderWriter *w, uint32_t value) { rs_write(w, &value, sizeof(value)); }
static inline const unsigned char *rs_take(RenderReader *r, uint32_t size)
{
    if (r->bad || size > r->size - r->pos) { r->bad = 1; return NULL; }
    const unsigned char *p = r->data + r->pos; r->pos += size; return p;
}
static inline void rs_read(RenderReader *r, void *out, uint32_t size)
{
    const void *p = rs_take(r, size);
    if (p && size) memcpy(out, p, size);
    else if (size) memset(out, 0, size);
}
static inline uint32_t rs_get_u32(RenderReader *r) { uint32_t x = 0; rs_read(r, &x, sizeof(x)); return x; }
static inline int rs_finish(RenderReader *r) { return !r->bad && r->pos == r->size; }
static inline int rs_publish(RenderWriter *w, SnapshotBlob *out)
{
    if (w->bad) { free(w->data); return 0; }
    out->data = w->data; out->size = w->size; return 1;
}
/* Each GL image is losslessly read as float from its supported storage format. */
typedef struct { uint32_t width, height, format, bytes; } RenderImage;
typedef struct { uint32_t tex; RenderImage image; } RenderStagedImage;
int gs_snapshot_write_texture(RenderWriter *w, unsigned int texture);
int gs_snapshot_read_texture(RenderReader *r, RenderStagedImage *out, int stage);
void gs_snapshot_drop_texture(RenderStagedImage *image);
void ntex_snapshot_write(RenderWriter *w);
void *ntex_snapshot_read(RenderReader *r, int stage);
void ntex_snapshot_commit(void *stage);
void ntex_snapshot_discard(void *stage);
void gs_parser_snapshot_write(RenderWriter *w);
void gs_parser_snapshot_read(RenderReader *r, int apply);
void gs_memory_snapshot_write(RenderWriter *w);
void gs_memory_snapshot_read(RenderReader *r, int apply);
void vif_snapshot_write(RenderWriter *w);
void vif_snapshot_read(RenderReader *r, int apply);
void stream_snapshot_write(RenderWriter *w);
void stream_snapshot_read(RenderReader *r, int apply);
int gs_snapshot_capture(SnapshotBlob *out);
int gs_snapshot_validate(const SnapshotBlob *in);
int gs_snapshot_restore(const SnapshotBlob *in);
#endif
