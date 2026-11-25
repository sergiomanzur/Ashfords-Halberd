package com.ashfordshalberd.app;

import android.test.InstrumentationTestCase;

import org.libsdl.app.SDL;

import java.io.File;
import java.lang.reflect.Method;

/**
 * On-device instrumentation harness for the native campaign tests that depend
 * on the Android Java lifecycle.
 *
 * The six SDL tests (pad_weapon_switch, pad_quicksave, rumble,
 * test_audio_snapshot, test_audio_snapshot_decoder, test_audio_snapshot_synth)
 * call SDL_Init(...), which on Android dereferences SDL's JavaVM registered by
 * JNI_OnLoad from a Java process; headless adb shell runs segfault. The
 * bindings test additionally guards its virtual-gamepad block behind
 * SDL_Init(SDL_INIT_GAMEPAD), so that block is dead headless.
 *
 * Each test body lives in its own libjni_test_<name>.so (staged only in this
 * androidTest APK), statically linking SDL3 with JNI_OnLoad exported, so
 * loading it registers SDL's Java bindings in this process before the test
 * main runs. The natives return the process exit code of the test main.
 */
public class NativeSDLTests extends InstrumentationTestCase {

    private static native int runBindings(String workdir);
    private static native int runPadWeaponSwitch(String workdir);
    private static native int runPadQuicksave(String workdir);
    private static native int runRumble(String workdir);
    private static native int runAudioSnapshot(String workdir);
    private static native int runAudioSnapshotDecoder(String workdir);
    private static native int runAudioSnapshotSynth(String workdir);

    private NativeTestActivity activity;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        activity = launchActivity("com.ashfordshalberd.app", NativeTestActivity.class, null);
        assertNotNull("NativeTestActivity launched", activity);
    }

    @Override
    protected void tearDown() throws Exception {
        if (activity != null) {
            activity.finish();
            activity = null;
        }
        super.tearDown();
    }

    private void runNativeTest(String library, String method) throws Throwable {
        // The native test bodies write scratch files into the working
        // directory; instrumentation processes start in a read-only cwd, so
        // the native entry chdirs into app-private storage first.
        File work = new File(getInstrumentation().getTargetContext().getFilesDir(), "jni-test-work");
        assertTrue("jni-test-work directory", work.exists() || work.mkdirs());

        System.loadLibrary("jni_test_" + library);
        SDL.setupJNI();
        SDL.initialize();
        // SDLActivity does this from its own onCreate; without a context the
        // Android file/audio JNI bridges dereference null (SDL.getContext()).
        SDL.setContext(activity);

        Method m = NativeSDLTests.class.getDeclaredMethod(method, String.class);
        m.setAccessible(true);
        int rc = (Integer) m.invoke(null, work.getAbsolutePath());
        assertEquals(library + " native test exit code", 0, rc);
    }

    public void testBindings() throws Throwable {
        runNativeTest("bindings", "runBindings");
    }

    public void testPadWeaponSwitch() throws Throwable {
        runNativeTest("pad_weapon_switch", "runPadWeaponSwitch");
    }

    public void testPadQuicksave() throws Throwable {
        runNativeTest("pad_quicksave", "runPadQuicksave");
    }

    public void testRumble() throws Throwable {
        runNativeTest("rumble", "runRumble");
    }

    public void testAudioSnapshot() throws Throwable {
        runNativeTest("audio_snapshot", "runAudioSnapshot");
    }

    public void testAudioSnapshotDecoder() throws Throwable {
        runNativeTest("audio_snapshot_decoder", "runAudioSnapshotDecoder");
    }

    public void testAudioSnapshotSynth() throws Throwable {
        runNativeTest("audio_snapshot_synth", "runAudioSnapshotSynth");
    }
}
