/* Android Storage Access Framework (SAF) ISO importer and immersive-mode JNI bridge. */
#ifndef ASHFORD_ANDROID_SAF_H
#define ASHFORD_ANDROID_SAF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ASHFORD_SAF_IDLE = 0,
    ASHFORD_SAF_PENDING = 1,
    ASHFORD_SAF_DONE = 2,
    ASHFORD_SAF_CANCELLED = 3,
    ASHFORD_SAF_ERROR = -1,
};

#if defined(__ANDROID__)
/* Launch the system ACTION_OPEN_DOCUMENT picker via AshfordActivity and stream-copy
 * the chosen ISO into <filesDir>/game/disc.iso. Returns 1 if started, 0 on error. */
int android_saf_pick_iso(void);

/* Poll importer state (returns ASHFORD_SAF_*). On DONE, fills path_out; on ERROR,
 * fills err_out; on PENDING, *pct_out is -1 while picker is open or 0..100 while copying.
 * Terminal states (DONE, CANCELLED, ERROR) reset back to IDLE once read. */
int android_saf_poll(char* path_out, size_t path_cap, char* err_out, size_t err_cap, int* pct_out);

/* Toggle Android immersive sticky fullscreen vs visible system bars / letterboxed presentation. */
void android_set_immersive(int immersive);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ASHFORD_ANDROID_SAF_H */
