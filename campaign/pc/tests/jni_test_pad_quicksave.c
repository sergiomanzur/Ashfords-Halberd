/* Android instrumentation wrapper for the quicksave input gate test. */
#include <jni.h>
#include "jni_test_entry.h"

#define main ashford_jni_test_main
#include "test_pad_quicksave.c"
#undef main

JNIEXPORT jint JNICALL Java_com_ashfordshalberd_app_NativeSDLTests_runPadQuicksave(JNIEnv *env, jclass cls, jstring workdir)
{
    ashford_jni_test_prepare(env, cls, workdir, "pad_quicksave");
    return ashford_jni_test_main();
}
