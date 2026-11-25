package com.ashfordshalberd.app;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

public class AshfordActivity extends SDLActivity {
    private static final String TAG = "AshfordsHalberd";
    private static final int REQ_SAF_ISO = 41024;
    private static final int COPY_BUF_BYTES = 1024 * 1024;

    private static native void nativeSafImportProgress(int pct);
    private static native void nativeSafImportFinished(String path, String error, boolean cancelled);

    @Override
    protected String[] getLibraries() {
        // SDL3 is linked statically into libmain.so (-Wl,-u,JNI_OnLoad).
        // librecvx_game.so is loaded explicitly at [0x30000000, 0x50000000) by
        // ashford_window_init() inside SDL_main and MUST NOT appear here.
        return new String[] { "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    protected String[] getArguments() {
        List<String> args = new ArrayList<>();
        Intent intent = getIntent();
        if (intent != null) {
            // Optional env passthrough for adb-driven verification:
            //   adb shell am start -n com.ashfordshalberd.app/.AshfordActivity \
            //       --es env "RECVX_DMA_MAP_AUDIT=1;RECVX_DUMP_FRAMES=/data/..."
            String envSpec = intent.getStringExtra("env");
            if (envSpec != null && !envSpec.isEmpty()) {
                for (String pair : envSpec.split(";")) {
                    int eq = pair.indexOf('=');
                    if (eq > 0) {
                        String k = pair.substring(0, eq).trim();
                        String v = pair.substring(eq + 1);
                        if (!k.isEmpty()) {
                            SDLActivity.nativeSetenv(k, v);
                        }
                    }
                }
            }
            if (intent.getBooleanExtra("skip_launcher", false)) {
                args.add("--skip-launcher");
            }
            String extraArgs = intent.getStringExtra("args");
            if (extraArgs != null && !extraArgs.trim().isEmpty()) {
                for (String tok : extraArgs.trim().split("\\s+")) {
                    if (!tok.isEmpty()) {
                        args.add(tok);
                    }
                }
            }
            String iso = intent.getStringExtra("iso");
            if (iso != null && !iso.isEmpty()) {
                args.add(iso);
            }
        }
        return args.toArray(new String[0]);
    }

    @SuppressWarnings("deprecation")
    public void setImmersiveMode(final boolean immersive) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                View decor = getWindow().getDecorView();
                if (immersive) {
                    decor.setSystemUiVisibility(
                        View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_FULLSCREEN);
                } else {
                    decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
                }
            }
        });
    }

    public void launchSafIsoPicker() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    startActivityForResult(intent, REQ_SAF_ISO);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to launch SAF picker", e);
                    nativeSafImportFinished(null, "Could not open file picker: " + e.getMessage(), false);
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQ_SAF_ISO) {
            if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
                nativeSafImportFinished(null, null, true);
                return;
            }
            final Uri uri = data.getData();
            startIsoCopyThread(uri);
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private long queryUriSize(Uri uri) {
        Cursor cursor = null;
        try {
            cursor = getContentResolver().query(uri, new String[] { OpenableColumns.SIZE }, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int idx = cursor.getColumnIndex(OpenableColumns.SIZE);
                if (idx >= 0 && !cursor.isNull(idx)) {
                    return cursor.getLong(idx);
                }
            }
        } catch (Exception ignored) {
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return -1L;
    }

    private void startIsoCopyThread(final Uri uri) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                File gameDir = new File(getFilesDir(), "game");
                if (!gameDir.exists() && !gameDir.mkdirs()) {
                    nativeSafImportFinished(null, "Cannot create app game directory.", false);
                    return;
                }
                File tmp = new File(gameDir, "disc.iso.tmp");
                File dest = new File(gameDir, "disc.iso");
                long totalSize = queryUriSize(uri);
                long copied = 0L;
                int lastPct = -1;
                nativeSafImportProgress(0);

                try (InputStream in = getContentResolver().openInputStream(uri);
                     FileOutputStream out = new FileOutputStream(tmp)) {
                    if (in == null) {
                        nativeSafImportFinished(null, "Cannot open selected disc image stream.", false);
                        return;
                    }
                    byte[] buf = new byte[COPY_BUF_BYTES];
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        out.write(buf, 0, n);
                        copied += n;
                        int pct = (totalSize > 0)
                            ? (int) Math.min(99L, (copied * 100L) / totalSize)
                            : (int) ((copied / (16L * 1024L * 1024L)) % 100L);
                        if (pct != lastPct) {
                            lastPct = pct;
                            nativeSafImportProgress(pct);
                        }
                    }
                    out.getFD().sync();
                } catch (Exception e) {
                    Log.e(TAG, "SAF ISO copy failed", e);
                    if (tmp.exists()) {
                        tmp.delete();
                    }
                    nativeSafImportFinished(null, "ISO import failed: " + e.getMessage(), false);
                    return;
                }

                if (dest.exists() && !dest.delete()) {
                    tmp.delete();
                    nativeSafImportFinished(null, "Cannot replace existing imported disc image.", false);
                    return;
                }
                if (!tmp.renameTo(dest)) {
                    tmp.delete();
                    nativeSafImportFinished(null, "Cannot finalize imported disc image.", false);
                    return;
                }
                nativeSafImportProgress(100);
                nativeSafImportFinished(dest.getAbsolutePath(), null, false);
            }
        }, "AshfordSafImport").start();
    }
}
