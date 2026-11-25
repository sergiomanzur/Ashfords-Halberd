/* Android JNI bridge for SAF ISO import and immersive-mode control. */
#if defined(__ANDROID__)
#include "android_saf.h"
#include "../ps2_platform.h"
#include <SDL3/SDL.h>
#include <jni.h>
#include <string.h>

static SDL_Mutex* g_saf_lock;
static int g_saf_state = ASHFORD_SAF_IDLE;
static int g_saf_pct = -1;
static char g_saf_path[1024];
static char g_saf_err[256];

static void saf_ensure_lock(void)
{
    if (!g_saf_lock)
        g_saf_lock = SDL_CreateMutex();
}

int android_saf_pick_iso(void)
{
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) {
        host_log("android_saf: JNI environment or activity unavailable\n");
        return 0;
    }

    saf_ensure_lock();
    if (g_saf_lock) SDL_LockMutex(g_saf_lock);
    g_saf_state = ASHFORD_SAF_PENDING;
    g_saf_pct = -1;
    g_saf_path[0] = '\0';
    g_saf_err[0] = '\0';
    if (g_saf_lock) SDL_UnlockMutex(g_saf_lock);

    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID mid = cls ? (*env)->GetMethodID(env, cls, "launchSafIsoPicker", "()V") : NULL;
    int ok = 0;
    if (mid) {
        (*env)->CallVoidMethod(env, activity, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        } else {
            ok = 1;
        }
    } else {
        if ((*env)->ExceptionCheck(env))
            (*env)->ExceptionClear(env);
        host_log("android_saf: launchSafIsoPicker() not found on Activity\n");
    }

    if (cls) (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);

    if (!ok) {
        if (g_saf_lock) SDL_LockMutex(g_saf_lock);
        g_saf_state = ASHFORD_SAF_ERROR;
        SDL_strlcpy(g_saf_err, "Could not launch the Android document picker.", sizeof(g_saf_err));
        if (g_saf_lock) SDL_UnlockMutex(g_saf_lock);
    }
    return ok;
}

int android_saf_poll(char* path_out, size_t path_cap, char* err_out, size_t err_cap, int* pct_out)
{
    saf_ensure_lock();
    if (g_saf_lock) SDL_LockMutex(g_saf_lock);
    int st = g_saf_state;
    if (pct_out) *pct_out = g_saf_pct;
    if (st == ASHFORD_SAF_DONE) {
        if (path_out && path_cap)
            SDL_strlcpy(path_out, g_saf_path, path_cap);
        g_saf_state = ASHFORD_SAF_IDLE;
    } else if (st == ASHFORD_SAF_ERROR) {
        if (err_out && err_cap)
            SDL_strlcpy(err_out, g_saf_err, err_cap);
        g_saf_state = ASHFORD_SAF_IDLE;
    } else if (st == ASHFORD_SAF_CANCELLED) {
        g_saf_state = ASHFORD_SAF_IDLE;
    }
    if (g_saf_lock) SDL_UnlockMutex(g_saf_lock);
    return st;
}

void android_set_immersive(int immersive)
{
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity)
        return;

    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID mid = cls ? (*env)->GetMethodID(env, cls, "setImmersiveMode", "(Z)V") : NULL;
    if (mid) {
        (*env)->CallVoidMethod(env, activity, mid, immersive ? JNI_TRUE : JNI_FALSE);
        if ((*env)->ExceptionCheck(env))
            (*env)->ExceptionClear(env);
    } else if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
    if (cls) (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);
}

JNIEXPORT void JNICALL
Java_com_ashfordshalberd_app_AshfordActivity_nativeSafImportProgress(JNIEnv* env, jclass cls, jint pct)
{
    (void)env;
    (void)cls;
    saf_ensure_lock();
    if (g_saf_lock) SDL_LockMutex(g_saf_lock);
    if (g_saf_state == ASHFORD_SAF_PENDING)
        g_saf_pct = (int)pct;
    if (g_saf_lock) SDL_UnlockMutex(g_saf_lock);
}

JNIEXPORT void JNICALL
Java_com_ashfordshalberd_app_AshfordActivity_nativeSafImportFinished(JNIEnv* env, jclass cls,
                                                                     jstring path, jstring error,
                                                                     jboolean cancelled)
{
    (void)cls;
    saf_ensure_lock();
    if (g_saf_lock) SDL_LockMutex(g_saf_lock);
    if (cancelled) {
        g_saf_state = ASHFORD_SAF_CANCELLED;
        g_saf_path[0] = '\0';
        g_saf_err[0] = '\0';
    } else if (path) {
        const char* utf = (*env)->GetStringUTFChars(env, path, NULL);
        if (utf) {
            SDL_strlcpy(g_saf_path, utf, sizeof(g_saf_path));
            (*env)->ReleaseStringUTFChars(env, path, utf);
            g_saf_state = ASHFORD_SAF_DONE;
            g_saf_pct = 100;
        } else {
            g_saf_state = ASHFORD_SAF_ERROR;
            SDL_strlcpy(g_saf_err, "Failed to read imported ISO path.", sizeof(g_saf_err));
        }
    } else {
        g_saf_state = ASHFORD_SAF_ERROR;
        if (error) {
            const char* utf = (*env)->GetStringUTFChars(env, error, NULL);
            if (utf) {
                SDL_strlcpy(g_saf_err, utf, sizeof(g_saf_err));
                (*env)->ReleaseStringUTFChars(env, error, utf);
            } else {
                SDL_strlcpy(g_saf_err, "Disc import failed.", sizeof(g_saf_err));
            }
        } else {
            SDL_strlcpy(g_saf_err, "Disc import failed.", sizeof(g_saf_err));
        }
    }
    if (g_saf_lock) SDL_UnlockMutex(g_saf_lock);
}
#endif /* defined(__ANDROID__) */
