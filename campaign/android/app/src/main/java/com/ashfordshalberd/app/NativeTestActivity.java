package com.ashfordshalberd.app;

import android.app.Activity;

/**
 * Minimal no-UI context holder launched by the instrumentation harness so
 * SDL's Android file/audio JNI bridges (SDLActivity.getContext()) have a
 * real Context inside the androidTest process. The real app supplies this
 * through SDLActivity; the harness must supply its own because no game
 * activity is ever started.
 */
public class NativeTestActivity extends Activity {
}
