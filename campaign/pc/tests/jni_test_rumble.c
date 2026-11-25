/* Android instrumentation wrapper for the rumble capability/motor test. */
#include <jni.h>
#include "jni_test_entry.h"

#define main ashford_jni_test_main
#include "test_rumble.c"
#undef main

JNIEXPORT jint JNICALL Java_com_ashfordshalberd_app_NativeSDLTests_runRumble(JNIEnv *env, jclass cls, jstring workdir)
{
    ashford_jni_test_prepare(env, cls, workdir, "rumble");
    return ashford_jni_test_main();
}
