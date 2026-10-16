package org.mrckr.switchruntime;

import android.app.Activity;
import android.os.Bundle;
import android.view.Window;
import android.view.WindowManager;

import java.io.File;

public final class MainActivity extends Activity {
    static { System.loadLibrary("switch_runtime"); }

    private native void nativeSetReportDir(String dir);
    private native void nativeSetSurface(android.view.Surface surface);
    private native void nativeSurfaceDestroyed();
    private native void nativeLoadGuest(String stagingDir);

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        // Diagnostic switches are read by the native layer via getenv. They are
        // only defaults: a build with DEBUG_LOG left on produces a lot of output.
        // Diagnostic switches are read by the native layer via getenv. overwrite=0
        // keeps any value the harness already set (e.g. via `am start --es`).
        try {
            android.system.Os.setenv("DM_SVC", "1", false);
            android.system.Os.setenv("DM_THR", "1", false);
            android.system.Os.setenv("DM_SAMPLE", "2", false);
            // DM_FS logs every file the guest opens; without it a silently
            // failing resource load looks like the guest simply doing nothing.
            android.system.Os.setenv("DM_FS", "1", false);
        } catch (Throwable ignored) { }
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                             WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // App-private storage: the report stays with the app and needs no permission.
        File base = getExternalFilesDir(null) != null
                ? getExternalFilesDir(null) : getFilesDir();
        File dir = new File(base, "diagnostics");
        if (dir.exists() || dir.mkdirs()) nativeSetReportDir(dir.getAbsolutePath());

        // Runtime file mounts.  The guest asks for "rom:/..." and the host
        // translates it to a real directory, so the resources and the save have
        // to be pointed at before the guest starts.
        //   rom  = the extracted RomFS (read-only, the game parses its archives)
        //   save = a writable directory; the guest writes its save containers here
        //   temp = scratch space
        //
        // save and temp MUST live on internal storage.  /storage/emulated/0/...
        // is served by the FUSE layer under scoped storage, and a plain mkdir(2)
        // against it fails with EOPNOTSUPP (95) — the guest's own "Diablo II"
        // directory could never be created there.  Internal storage is a real
        // filesystem, so the syscalls the guest makes actually work.
        //
        // The RomFS is the exception: it is 8.4 GB and has to be read from where
        // it was pushed, so it stays on external storage and is only ever read.
        File rom = new File(base, "romfs");
        File save = new File(getFilesDir(), "save");
        File temp = new File(getFilesDir(), "temp");
        for (File d : new File[]{save, temp}) { if (!d.exists()) d.mkdirs(); }
        try {
            android.system.Os.setenv("DM_ROMFS", rom.getAbsolutePath(), true);
            android.system.Os.setenv("DM_SAVE", save.getAbsolutePath(), true);
            android.system.Os.setenv("DM_TEMP", temp.getAbsolutePath(), true);
            android.system.Os.setenv("TMPDIR", temp.getAbsolutePath(), true);
        } catch (Throwable ignored) { }
        android.util.Log.i("SwitchRuntime", "mounts: rom=" + rom + " save=" + save
                + " saveWritable=" + save.canWrite());

        // Optional local staging smoke test. The directory is populated by the
        // developer build/device harness; an absent directory is not an error.
        File guest = new File(base, "guest");
        if (new File(guest, "main.patched.bin").isFile()
                && new File(guest, "main.fixups.tsv").isFile()
                && new File(guest, "main.gates.tsv").isFile()
                && new File(guest, "sdk.patched.bin").isFile()
                && new File(guest, "sdk.fixups.tsv").isFile()
                && new File(guest, "subsdk0.patched.bin").isFile()
                && new File(guest, "subsdk0.fixups.tsv").isFile()) {
            nativeLoadGuest(guest.getAbsolutePath());
        } else {
            android.util.Log.i("SwitchRuntime", "guest staging incomplete: need main, sdk, and subsdk0 rewritten modules");
        }

        setContentView(new RuntimeView(this));
    }

    private final class RuntimeView extends android.view.SurfaceView
            implements android.view.SurfaceHolder.Callback {
        RuntimeView(android.content.Context c) { super(c); getHolder().addCallback(this); }
        @Override public void surfaceCreated(android.view.SurfaceHolder h) { nativeSetSurface(h.getSurface()); }
        @Override public void surfaceChanged(android.view.SurfaceHolder h, int f, int w, int hh) { nativeSetSurface(h.getSurface()); }
        @Override public void surfaceDestroyed(android.view.SurfaceHolder h) { nativeSurfaceDestroyed(); }
    }
}
