/* Android instrumentation wrapper for the audio snapshot roundtrip test.
   Runs the non-silent path (SDL_INIT_AUDIO with the dummy driver). */
#include <jni.h>
#include "jni_test_entry.h"

#define main ashford_jni_test_main
#include "test_audio_snapshot.c"
#undef main

JNIEXPORT jint JNICALL Java_com_ashfordshalberd_app_NativeSDLTests_runAudioSnapshot(JNIEnv *env, jclass cls, jstring workdir)
{
    ashford_jni_test_prepare(env, cls, workdir, "audio_snapshot");
    static char arg0[] = "test_audio_snapshot";
    char *argv[] = { arg0, NULL };
    return ashford_jni_test_main(1, argv);
}
