/* Android instrumentation wrapper for the audio decoder snapshot test. */
#include <jni.h>
#include "jni_test_entry.h"

#define main ashford_jni_test_main
#include "test_audio_snapshot_decoder.c"
#undef main

JNIEXPORT jint JNICALL Java_com_ashfordshalberd_app_NativeSDLTests_runAudioSnapshotDecoder(JNIEnv *env, jclass cls, jstring workdir)
{
    ashford_jni_test_prepare(env, cls, workdir, "audio_snapshot_decoder");
    return ashford_jni_test_main();
}
