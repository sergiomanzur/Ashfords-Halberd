#ifndef RECVX_PRESETS_H
#define RECVX_PRESETS_H
#ifdef __cplusplus
extern "C" {
#endif

/* Original (0) is an original-style presentation/QOL baseline, Enhanced (1)
   enables the selected improvements, Custom (2) leaves every option alone.
   These pure rules change configuration only; the caller applies and saves it. */
int preset_get(void);
void preset_set(int value);

#ifdef __cplusplus
}
#endif
#endif
