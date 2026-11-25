/* Shared helper for the Android instrumentation test wrappers: chdir into a
   writable scratch directory (instrumentation processes start in a read-only
   cwd and the test bodies write scratch files) and mirror the test's stdout
   into a log file there, since a native printf inside an app process is
   otherwise invisible to adb. */
#ifndef ASHFORD_JNI_TEST_ENTRY_H
#define ASHFORD_JNI_TEST_ENTRY_H

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static void ashford_jni_test_prepare(JNIEnv *env, jclass cls, jstring workdir, const char *name)
{
    (void)cls;
    if (!workdir) return;
    const char *dir = (*env)->GetStringUTFChars(env, workdir, NULL);
    if (!dir) return;
    chdir(dir);
    char path[512];
    snprintf(path, sizeof(path), "%s/jni_test_%s.log", dir, name);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        dup2(fd, 1);
        if (fd != 1) close(fd);
    }
    (*env)->ReleaseStringUTFChars(env, workdir, dir);
}

#endif
