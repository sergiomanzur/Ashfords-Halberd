/* Android instrumentation wrapper for the portable bindings test.
   The test body is reused verbatim; only the entry point changes. Each
   libjni_test_<name>.so links SDL3 statically with JNI_OnLoad exported so
   System.loadLibrary() in the androidTest process bootstraps SDL's JavaVM
   and Java bindings before the test main runs. */
#include <jni.h>
#include "jni_test_entry.h"

#define main ashford_jni_test_main
#include "test_bindings.c"
#undef main

JNIEXPORT jint JNICALL Java_com_ashfordshalberd_app_NativeSDLTests_runBindings(JNIEnv *env, jclass cls, jstring workdir)
{
    ashford_jni_test_prepare(env, cls, workdir, "bindings");
    return ashford_jni_test_main();
}
