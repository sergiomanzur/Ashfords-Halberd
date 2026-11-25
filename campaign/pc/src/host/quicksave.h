#ifndef ASHFORD_QUICKSAVE_H
#define ASHFORD_QUICKSAVE_H
#ifdef __cplusplus
extern "C" {
#endif
enum {
    QS_NONE, QS_BLOCKED, QS_COMBAT, QS_TIMER, QS_MISSING, QS_CORRUPT,
    QS_WRITE_FAILED, QS_SAVED, QS_LOADING, QS_RESTORED, QS_BAD_STATE,
    QS_BOTH, QS_READ_FAILED, QS_RESTORE_FAILED, QS_WRONG_BUILD, QS_UNAVAILABLE
};
void quicksave_notice_set(int message);
const char* quicksave_notice(void);
const char* quicksave_path(void);
/* Lifecycle hook (main/render thread -> game thread): request a silent safety quicksave when legal. */
void quicksave_request_auto(void);
int  quicksave_auto_pending(void);
int  quicksave_take_auto(void);
/* Game thread: input sample (even in menus), then end-of-frame transaction. */
void quicksave_pad(void);
void quicksave_frame(void);
#ifdef __cplusplus
}
#endif
#endif
