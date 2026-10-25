/*
 * Audio output (SDL3). Each ADX stream is an SDL_AudioStream bound to the output
 * device; SDL mixes them. Streams decode on demand on SDL's audio thread, reading
 * the disc image through their own file handle.
 *
 * CRI ADX (type 3): 18-byte frames per channel (BE16 scale + 32 4-bit samples),
 * channels interleaved per frame, 2nd-order prediction with coefficients derived
 * from the header's high-pass cutoff. Versions 3/4 carry loop points.
 */
#include "../ps2_platform.h"
#include "audio.h"
#include "snd.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_VOICES 8
#define SECTOR 2048
#define ADX_PCM_HISTORY 32768

typedef struct {
    int used;
    SDL_AudioStream* stream;
    FILE* f;
    uint64_t base;               /* byte offset of the ADX file in the image */
    uint32_t size;
    /* header */
    uint32_t channels, rate, total, data_ofs, frame_bytes;
    int loop;
    uint32_t loop_start_sample, loop_start_byte, loop_end_sample;
    int16_t coef1, coef2;
    /* decoder state */
    uint32_t pos_byte, decoded;  /* next frame offset (relative to file), samples decoded */
    int32_t hist1[8], hist2[8];
    int ended;
    int vol_cdb;
    int source_iso, paused;
    SDL_AudioSpec output_spec;
    char path[1024];
    uint32_t pcm_frames;
    int16_t pcm[ADX_PCM_HISTORY * 2]; /* recent source-format PCM, for restoring queued input */
    uint64_t silent_start_ns, silent_pos;
} AdxVoice;

static SDL_AudioDeviceID dev;
static AdxVoice voices[MAX_VOICES];
static int mono_out;
/* The game thread starts/stops voices and the movie stream; the main thread applies the player
   volumes (F1, settings_poll). This lock covers a stream's publication (used / movie_stream),
   its gain and its removal, so the main thread never sets the gain of a destroyed stream. */
static SDL_Mutex* lock;

static uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

void audio_init(void)
{
    SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 48000 };
    lock = SDL_CreateMutex();   /* before the game thread starts */
    dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!dev) host_log("audio: cannot open output device: %s (continuing silently)\n", SDL_GetError());
    snd_synth_init(dev);
    audio_apply_volumes();
}

void audio_set_mono(int mono) { mono_out = mono; }

static float gain_of(int vol_cdb)
{
    return vol_cdb <= -999 ? 0.0f : powf(10.0f, (float)vol_cdb / 200.0f) * g_config.music_volume / 100.0f;
}

static SDL_AudioStream* movie_stream;
static float movie_gain = 1.0f;

int audio_movie_open(int rate)
{
    SDL_AudioSpec src = { SDL_AUDIO_S16, 2, rate }, dst = { SDL_AUDIO_S16, 2, 48000 };
    SDL_AudioStream* s;
    if (!dev) return 0;
    s = SDL_CreateAudioStream(&src, &dst);
    SDL_LockMutex(lock);
    movie_stream = s;
    if (s) SDL_SetAudioStreamGain(s, movie_gain * g_config.music_volume / 100.0f);
    SDL_UnlockMutex(lock);
    return s != NULL;
}
void audio_movie_put(const int16_t* s, size_t frames) { if (movie_stream) SDL_PutAudioStreamData(movie_stream, s, (int)(frames * 4)); }
int audio_movie_queued(void) { return movie_stream ? SDL_GetAudioStreamQueued(movie_stream) : 0; }
void audio_movie_pause(int paused)
{
    if (!movie_stream) return;
    if (paused) SDL_UnbindAudioStream(movie_stream); else SDL_BindAudioStream(dev, movie_stream);
}
void audio_movie_volume(float gain)
{
    SDL_LockMutex(lock);
    movie_gain = gain;
    if (movie_stream) SDL_SetAudioStreamGain(movie_stream, movie_gain * g_config.music_volume / 100.0f);
    SDL_UnlockMutex(lock);
}
float audio_movie_gain(void) { return movie_gain * g_config.music_volume / 100.0f; }
void audio_movie_close(void)
{
    SDL_AudioStream* s;
    SDL_LockMutex(lock);
    s = movie_stream;
    movie_stream = NULL;
    SDL_UnlockMutex(lock);
    if (!s) return;
    SDL_UnbindAudioStream(s);
    SDL_DestroyAudioStream(s);
}

/* main thread (F1, settings_poll) */
void audio_apply_volumes(void)
{
    SDL_LockMutex(lock);
    if (movie_stream) SDL_SetAudioStreamGain(movie_stream, movie_gain * g_config.music_volume / 100.0f);
    for (int i = 0; i < MAX_VOICES; i++) if (voices[i].used) SDL_SetAudioStreamGain(voices[i].stream, gain_of(voices[i].vol_cdb));
    SDL_UnlockMutex(lock);
    snd_synth_set_volume(g_config.sfx_volume / 100.0f);
}

/* decode one frame per channel (32 samples) into out (interleaved s16) */
static int decode_frames(AdxVoice* v, int16_t* out)
{
    uint8_t buf[8 * 18];
    uint32_t n = v->channels * v->frame_bytes;
    if (v->loop && v->decoded >= v->loop_end_sample) { v->pos_byte = v->loop_start_byte; v->decoded = v->loop_start_sample; }
    if (!v->loop && v->decoded >= v->total) return 0;
    if (v->pos_byte + n > v->size) return 0;
    if (fseeko(v->f, (off_t)(v->base + v->pos_byte), SEEK_SET) != 0 || fread(buf, 1, n, v->f) != n) return 0;
    v->pos_byte += n;
    for (uint32_t ch = 0; ch < v->channels; ch++) {
        const uint8_t* fr = buf + ch * v->frame_bytes;
        int32_t scale = (int16_t)be16(fr) + 1;
        for (int i = 0; i < 32; i++) {
            int nib = (fr[2 + i / 2] >> ((i & 1) ? 0 : 4)) & 15;
            int32_t s;
            if (nib & 8) nib -= 16;
            s = nib * scale + ((v->coef1 * v->hist1[ch]) >> 12) + ((v->coef2 * v->hist2[ch]) >> 12);
            if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
            v->hist2[ch] = v->hist1[ch]; v->hist1[ch] = s;
            out[i * 2 + 0 + (v->channels > 1 ? ch : 0)] = (int16_t)s;
            if (v->channels == 1) out[i * 2 + 1] = (int16_t)s;
        }
    }
    {   /* clip the last frame at the end of the file */
        int samples = 32;
        if (!v->loop && v->decoded + 32 > v->total) samples = (int)(v->total - v->decoded);
        if (v->loop && v->decoded + 32 > v->loop_end_sample) samples = (int)(v->loop_end_sample - v->decoded);
        v->decoded += (uint32_t)samples;
        if (mono_out) for (int i = 0; i < samples; i++) { int16_t m = (int16_t)((out[i * 2] + out[i * 2 + 1]) / 2); out[i * 2] = out[i * 2 + 1] = m; }
        return samples;
    }
}

static void SDLCALL feed(void* userdata, SDL_AudioStream* stream, int additional, int total)
{
    AdxVoice* v = userdata;
    int16_t pcm[64 * 32 * 2];
    (void)total;
    while (additional > 0 && !v->ended) {
        int got = 0;
        while (got < 64 * 32 - 32) {
            int n = decode_frames(v, pcm + got * 2);
            if (n <= 0) { v->ended = 1; break; }
            got += n;
        }
        if (got) {
            unsigned keep = v->pcm_frames;
            if (keep + (unsigned)got > ADX_PCM_HISTORY) { unsigned drop = keep + (unsigned)got - ADX_PCM_HISTORY; memmove(v->pcm, v->pcm + drop * 2, (keep - drop) * 4); keep -= drop; }
            memcpy(v->pcm + keep * 2, pcm, got * 4); v->pcm_frames = keep + got;
            SDL_PutAudioStreamData(stream, pcm, got * 4);
        }
        additional -= got * 4;
    }
    if (v->ended) SDL_FlushAudioStream(stream);
}

int audio_stream_start_adx(const char* path, uint64_t base, uint32_t size, int vol_cdb)
{
    uint8_t hdr[SECTOR] = { 0 };
    AdxVoice* v = NULL;
    int id = -1;
    for (int i = 0; i < MAX_VOICES; i++) if (!voices[i].used) { v = &voices[i]; id = i; break; }
    if (!v || !dev) return -1;
    memset(v, 0, sizeof(*v));
    v->f = pc_fopen(path, "rb");
    if (!v->f) return -1;
    v->base = base; v->size = size; v->source_iso = !strcmp(path, g_config.iso_path);
    if (!v->source_iso) SDL_strlcpy(v->path, path, sizeof(v->path));
    if (fseeko(v->f, (off_t)v->base, SEEK_SET) != 0 || fread(hdr, 1, SECTOR, v->f) < 0x34 || hdr[0] != 0x80) { fclose(v->f); return -1; }
    v->data_ofs = be16(hdr + 2) + 4;
    v->frame_bytes = hdr[5] ? hdr[5] : 18;
    v->channels = hdr[7] ? hdr[7] : 1;
    v->rate = be32(hdr + 8);
    v->total = be32(hdr + 12);
    {
        double cutoff = be16(hdr + 16), a, b, c;
        a = M_SQRT2 - cos(2.0 * M_PI * cutoff / v->rate);
        b = M_SQRT2 - 1.0;
        c = (a - sqrt((a + b) * (a - b))) / b;
        v->coef1 = (int16_t)(c * 8192.0);
        v->coef2 = (int16_t)(c * c * -4096.0);
    }
    if (hdr[0x12] == 3 && be32(hdr + 0x18)) {
        v->loop = 1; v->loop_start_sample = be32(hdr + 0x1C); v->loop_start_byte = be32(hdr + 0x20); v->loop_end_sample = be32(hdr + 0x24);
    } else if (hdr[0x12] == 4 && be32(hdr + 0x24)) {
        v->loop = 1; v->loop_start_sample = be32(hdr + 0x28); v->loop_start_byte = be32(hdr + 0x2C); v->loop_end_sample = be32(hdr + 0x30);
    }
    if (v->channels > 2 || v->frame_bytes != 18) {   /* decode_frames: 2 channels of 18-byte frames */
        host_log("adx: %u channels of %u-byte frames not supported\n", v->channels, v->frame_bytes);
        fclose(v->f);
        return -1;
    }
    v->pos_byte = v->data_ofs;
    {
        SDL_AudioSpec src = { SDL_AUDIO_S16, 2, (int)v->rate };
        SDL_AudioSpec dst = { SDL_AUDIO_S16, 2, 48000 };
        v->stream = SDL_CreateAudioStream(&src, &dst);
    }
    if (!v->stream) { fclose(v->f); return -1; }
    v->vol_cdb = vol_cdb;
    SDL_SetAudioStreamGetCallback(v->stream, feed, v);
    SDL_LockMutex(lock);
    SDL_SetAudioStreamGain(v->stream, gain_of(vol_cdb));
    v->used = 1;
    SDL_UnlockMutex(lock);
    SDL_BindAudioStream(dev, v->stream);
    return id;
}

void audio_stream_stop(int id)
{
    AdxVoice* v = &voices[id];
    if (id < 0 || id >= MAX_VOICES || !v->used) return;
    SDL_LockMutex(lock);
    v->used = 0;   /* unpublished: the main thread no longer touches it */
    SDL_UnlockMutex(lock);
    SDL_UnbindAudioStream(v->stream);
    SDL_DestroyAudioStream(v->stream);
    fclose(v->f);
}

void audio_stream_pause(int id, int paused) {
    if (id < 0 || id >= MAX_VOICES || !voices[id].used) return;
    AdxVoice* v = &voices[id]; v->paused = !!paused;
    if (v->stream) { if (paused) SDL_UnbindAudioStream(v->stream); else if (dev) SDL_BindAudioStream(dev, v->stream); }
}

void audio_stream_volume(int id, int vol_cdb)
{
    if (id < 0 || id >= MAX_VOICES || !voices[id].used) return;
    SDL_LockMutex(lock);
    voices[id].vol_cdb = vol_cdb;
    SDL_SetAudioStreamGain(voices[id].stream, gain_of(vol_cdb));
    SDL_UnlockMutex(lock);
}

/* samples actually played = decoded - still queued (converted back to the source rate) */
uint64_t audio_stream_position(int id)
{
    AdxVoice* v = &voices[id];
    int queued;
    uint64_t pos;
    if (id < 0 || id >= MAX_VOICES || !v->used) return 0;
    if (!dev) return v->silent_pos + (v->paused ? 0 : (SDL_GetTicksNS() - v->silent_start_ns) * v->rate / 1000000000ull);
    SDL_LockAudioStream(v->stream);
    queued = SDL_GetAudioStreamQueued(v->stream) / 4;
    pos = v->decoded > (uint32_t)queued ? v->decoded - (uint32_t)queued : 0;
    SDL_UnlockAudioStream(v->stream);
    return pos;
}

int audio_stream_finished(int id)
{
    AdxVoice* v = &voices[id];
    int done;
    if (id < 0 || id >= MAX_VOICES || !v->used) return 1;
    if (!dev) return !v->loop && audio_stream_position(id) >= v->total;
    SDL_LockAudioStream(v->stream);
    done = v->ended && SDL_GetAudioStreamAvailable(v->stream) == 0;
    SDL_UnlockAudioStream(v->stream);
    return done;
}

#include "audio_snapshot.h"
/* The stream's unconsumed source PCM is reconstructed along with decoder history.
   SDL device/resampler handles are always new; no FILE/SDL addresses enter the image. */
typedef struct { uint32_t version, bytes; AdxVoice voices[MAX_VOICES]; } DecoderSnapshot;
typedef struct { DecoderSnapshot state; FILE* files[MAX_VOICES]; SDL_AudioStream* streams[MAX_VOICES]; } DecoderStage;
static int decoder_capture(SnapshotBlob* out) {
    DecoderSnapshot* p = calloc(1, sizeof(*p)); if (!p) return 0; p->version = 1; p->bytes = sizeof(*p);
    SDL_LockMutex(lock);
    int ok = !movie_stream; /* movie decoder is intentionally outside normal room control */
    for (int i = 0; ok && i < MAX_VOICES; ++i) if (voices[i].used) {
        AdxVoice* v = &voices[i]; SDL_LockAudioStream(v->stream); p->voices[i] = *v;
        SDL_GetAudioStreamFormat(v->stream, NULL, &p->voices[i].output_spec);
        int queued = SDL_GetAudioStreamQueued(v->stream);
        if (queued < 0 || (queued & 3) || (unsigned)queued / 4 > v->pcm_frames) ok = 0;
        else {
            unsigned n = queued / 4; AdxVoice* dst = &p->voices[i];
            memmove(dst->pcm, dst->pcm + (dst->pcm_frames - n) * 2, n * 4);
            memset(dst->pcm + n * 2, 0, sizeof(dst->pcm) - n * 4); dst->pcm_frames = n;
            dst->silent_pos = audio_stream_position(i); dst->silent_start_ns = 0;
            dst->stream = NULL; dst->f = NULL;
        }
        SDL_UnlockAudioStream(v->stream);
    }
    SDL_UnlockMutex(lock);
    if (!ok) { free(p); return 0; } out->data = (unsigned char*)p; out->size = sizeof(*p); return 1;
}
static int decoder_validate(const SnapshotBlob* in) {
    if (!in || !in->data || in->size != sizeof(DecoderSnapshot)) return 0;
    DecoderSnapshot* p = malloc(sizeof(*p)); if (!p) return 0; memcpy(p, in->data, sizeof(*p)); int ok = p->version == 1 && p->bytes == sizeof(*p);
    for (int i = 0; ok && i < MAX_VOICES; ++i) {
        AdxVoice* v = &p->voices[i];
        if (v->stream || v->f || (v->used != 0 && v->used != 1) || !memchr(v->path, 0, sizeof(v->path))) { ok = 0; break; }
        if (!v->used) continue;
        if ((v->source_iso != 0 && v->source_iso != 1) || (!v->source_iso && !v->path[0]) || v->output_spec.channels < 1 || v->output_spec.channels > 8 || v->output_spec.freq < 8000 || v->output_spec.freq > 384000 || !SDL_AUDIO_BITSIZE(v->output_spec.format) || v->channels < 1 || v->channels > 2 || !v->rate || v->rate > 384000 || v->frame_bytes != 18 || v->size > AUDIO_SNAPSHOT_LIMIT || v->data_ofs > v->size || v->pos_byte > v->size || v->pcm_frames > ADX_PCM_HISTORY || v->base > UINT64_MAX - v->size || (v->loop && (v->loop_start_sample >= v->loop_end_sample || v->loop_end_sample > v->total || v->loop_start_byte > v->size))) ok = 0;
    }
    for (int i = 0; ok && i < MAX_VOICES; ++i) for (int ch = 0; ch < 8; ++ch) if (p->voices[i].hist1[ch] < -32768 || p->voices[i].hist1[ch] > 32767 || p->voices[i].hist2[ch] < -32768 || p->voices[i].hist2[ch] > 32767) ok = 0;
    free(p); return ok;
}
static void decoder_discard(DecoderStage* s) { if (!s) return; for (int i = 0; i < MAX_VOICES; ++i) { if (s->streams[i]) SDL_DestroyAudioStream(s->streams[i]); if (s->files[i]) fclose(s->files[i]); } free(s); }
static DecoderStage* decoder_stage(const SnapshotBlob* in) {
    if (!decoder_validate(in)) return NULL;
    DecoderStage* s = calloc(1, sizeof(*s)); if (!s) return NULL; memcpy(&s->state, in->data, sizeof(s->state));
    for (int i = 0; i < MAX_VOICES; ++i) if (s->state.voices[i].used) {
        AdxVoice* v = &s->state.voices[i]; s->files[i] = pc_fopen(v->source_iso ? g_config.iso_path : v->path, "rb"); if (!s->files[i]) goto fail;
        if (fseeko(s->files[i], 0, SEEK_END) || ftello(s->files[i]) < 0 || (uint64_t)ftello(s->files[i]) < v->base + v->size) goto fail;
        SDL_AudioSpec src = { SDL_AUDIO_S16, 2, (int)v->rate }, dst = v->output_spec;
        s->streams[i] = SDL_CreateAudioStream(&src, &dst); if (!s->streams[i]) goto fail;
        if (v->pcm_frames && !SDL_PutAudioStreamData(s->streams[i], v->pcm, v->pcm_frames * 4)) goto fail;
    }
    return s;
fail: decoder_discard(s); return NULL;
}
static void decoder_commit(DecoderStage* s) {
    for (int i = 0; i < MAX_VOICES; ++i) audio_stream_stop(i);
    SDL_LockMutex(lock);
    memcpy(voices, s->state.voices, sizeof(voices));
    for (int i = 0; i < MAX_VOICES; ++i) if (voices[i].used) {
        AdxVoice* v = &voices[i]; v->f = s->files[i]; s->files[i] = NULL; v->stream = s->streams[i]; s->streams[i] = NULL;
        v->silent_start_ns = SDL_GetTicksNS(); SDL_SetAudioStreamGain(v->stream, gain_of(v->vol_cdb)); SDL_SetAudioStreamGetCallback(v->stream, feed, v);
        if (dev && !v->paused) SDL_BindAudioStream(dev, v->stream);
    }
    SDL_UnlockMutex(lock); decoder_discard(s);
}
#define AUDIO_PARTS 5
static int audio_parts(const SnapshotBlob* in, SnapshotBlob part[AUDIO_PARTS]) {
    uint32_t header[2 + AUDIO_PARTS];
    if (!in || !in->data || in->size > AUDIO_SNAPSHOT_LIMIT) return 0;
    AudioReader r = {in->data, in->size, 1}; as_get(&r, header, sizeof(header));
    if (!r.ok || header[0] != 0x41555331u || header[1] != AUDIO_PARTS) return 0;
    for (int i = 0; i < AUDIO_PARTS; ++i) { if (header[2+i] > r.n) return 0; part[i] = (SnapshotBlob){(unsigned char*)r.p, header[2+i]}; as_get(&r, NULL, header[2+i]); }
    return r.ok && r.n == 0;
}
int snapshot_audio_capture(SnapshotBlob* out) {
    SnapshotBlob part[AUDIO_PARTS] = {{0}}; AudioWriter w = { .ok = 1 }; uint32_t h[2 + AUDIO_PARTS] = {0x41555331u, AUDIO_PARTS};
    if (!out) return 0;
    out->data = NULL; out->size = 0;
    int resume = dev && !SDL_AudioDevicePaused(dev);
    if (resume && !SDL_PauseAudioDevice(dev)) return 0;
    if (!decoder_capture(&part[0]) || !adx_snapshot_capture(&part[1]) || !snd_snapshot_capture(&part[2]) || !snd_iop_snapshot_capture(&part[3]) || !sif_snapshot_capture(&part[4])) w.ok = 0;
    for (int i = 0; i < AUDIO_PARTS; ++i) h[2+i] = part[i].size;
    as_put(&w, h, sizeof(h)); for (int i = 0; i < AUDIO_PARTS; ++i) { as_put(&w, part[i].data, part[i].size); free(part[i].data); }
    if (resume) SDL_ResumeAudioDevice(dev);
    return as_finish(&w, out);
}
int snapshot_audio_validate(const SnapshotBlob* in) {
    SnapshotBlob p[AUDIO_PARTS]; return audio_parts(in, p) && decoder_validate(&p[0]) && adx_snapshot_validate(&p[1]) && snd_snapshot_validate(&p[2]) && snd_iop_snapshot_validate(&p[3]) && sif_snapshot_validate(&p[4]);
}
typedef struct { DecoderStage* decoder; void *adx, *synth, *iop, *sif; int resume; } AudioSnapshotStage;
void snapshot_audio_discard(void* stage) {
    AudioSnapshotStage* s = stage; if (!s) return;
    decoder_discard(s->decoder); adx_snapshot_discard(s->adx); snd_snapshot_discard(s->synth); snd_iop_snapshot_discard(s->iop); sif_snapshot_discard(s->sif);
    if (s->resume) SDL_ResumeAudioDevice(dev);
    free(s);
}
void* snapshot_audio_prepare(const SnapshotBlob* in) {
    SnapshotBlob p[AUDIO_PARTS];
    if (!audio_parts(in, p) || !snapshot_audio_validate(in)) return NULL;
    AudioSnapshotStage* s = calloc(1, sizeof(*s)); if (!s) return NULL;
    s->decoder = decoder_stage(&p[0]); s->adx = adx_snapshot_stage(&p[1]); s->synth = snd_snapshot_stage(&p[2]); s->iop = snd_iop_snapshot_stage(&p[3]); s->sif = sif_snapshot_stage(&p[4]);
    if (!s->decoder || !s->adx || !s->synth || !s->iop || !s->sif) { snapshot_audio_discard(s); return NULL; }
    if (dev && !SDL_AudioDevicePaused(dev)) {
        if (!SDL_PauseAudioDevice(dev)) { snapshot_audio_discard(s); return NULL; }
        s->resume = 1;
    }
    return s;
}
void snapshot_audio_commit(void* stage) {
    AudioSnapshotStage* s = stage; if (!s) return;
    audio_movie_close();
    decoder_commit(s->decoder); s->decoder = NULL;
    adx_snapshot_commit(s->adx); s->adx = NULL;
    snd_snapshot_commit(s->synth); s->synth = NULL;
    snd_iop_snapshot_commit(s->iop); s->iop = NULL;
    sif_snapshot_commit(s->sif); s->sif = NULL;
    audio_apply_volumes(); snapshot_audio_discard(s);
}
int snapshot_audio_restore(const SnapshotBlob* in) {
    void* stage = snapshot_audio_prepare(in); if (!stage) return 0; snapshot_audio_commit(stage); return 1;
}
