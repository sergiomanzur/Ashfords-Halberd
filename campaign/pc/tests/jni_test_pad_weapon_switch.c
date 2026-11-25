/* Android instrumentation wrapper for the weapon-switch input gate test. */
#include <jni.h>
#include "jni_test_entry.h"

#define main ashford_jni_test_main
#include "test_pad_weapon_switch.c"
#undef main

JNIEXPORT jint JNICALL Java_com_ashfordshalberd_app_NativeSDLTests_runPadWeaponSwitch(JNIEnv *env, jclass cls, jstring workdir)
{
    ashford_jni_test_prepare(env, cls, workdir, "pad_weapon_switch");
    return ashford_jni_test_main();
}
