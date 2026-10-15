package org.mrckr.switchruntime;

import android.app.Activity;
import android.os.Bundle;
import android.view.Window;
import android.view.WindowManager;

public final class MainActivity extends Activity {
    static { System.loadLibrary("switch_runtime"); }
    private native void nativeSetSurface(android.view.Surface surface);
    private native void nativeSurfaceDestroyed();

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(new RuntimeView(this));
    }

    private final class RuntimeView extends android.view.SurfaceView implements android.view.SurfaceHolder.Callback {
        RuntimeView(android.content.Context c) { super(c); getHolder().addCallback(this); }
        @Override public void surfaceCreated(android.view.SurfaceHolder h) { nativeSetSurface(h.getSurface()); }
        @Override public void surfaceChanged(android.view.SurfaceHolder h, int f, int w, int hh) { nativeSetSurface(h.getSurface()); }
        @Override public void surfaceDestroyed(android.view.SurfaceHolder h) { nativeSurfaceDestroyed(); }
    }
}
