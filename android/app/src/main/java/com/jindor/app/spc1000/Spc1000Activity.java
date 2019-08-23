package com.jindor.app.spc1000;

import android.app.DialogFragment;
import android.content.DialogInterface;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;
import com.rustamg.filedialogs.FileDialog;
import com.rustamg.filedialogs.OpenFileDialog;
import com.rustamg.filedialogs.SaveFileDialog;

import java.io.File;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Main activity interfacing file IO user interface.
 */
public class Spc1000Activity
    extends SDLActivity implements FileDialog.OnFileSelectedListener {
  private final CountDownLatch mLatch = new CountDownLatch(1);

  // Used to pass the filename string from UI to SDL thread.
  private final AtomicReference<String> mFilename = new AtomicReference<>();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    Window window = getWindow();
    int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
    window.getDecorView().setSystemUiVisibility(flags);
    window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
    nativeInit(getResources().getAssets());
  }

  @Override
  public void onResume() {
    super.onResume();
  }

  // Should be public static for the underlying framework to handle the
  // lifecycle.
  public static class SDLOpenFileDialog extends OpenFileDialog {
    private CountDownLatch mDismissLatch;

    public SDLOpenFileDialog(CountDownLatch latch) { mDismissLatch = latch; }

    @Override
    public void onDismiss(DialogInterface dialog) {
      mDismissLatch.countDown();
    }
  }

  public static class SDLSaveFileDialog extends SaveFileDialog {
    private CountDownLatch mDismissLatch;

    public SDLSaveFileDialog(CountDownLatch latch) { mDismissLatch = latch; }

    @Override
    public void onDismiss(DialogInterface dialog) {
      mDismissLatch.countDown();
    }
  }

  private String fileDialog(boolean open, String ext) {
    FileDialog dialog =
        open ? new SDLOpenFileDialog(mLatch) : new SDLSaveFileDialog(mLatch);
    mFilename.set(null);
    if (!open && !TextUtils.isEmpty(ext)) {
      Bundle args = new Bundle();
      args.putString(FileDialog.EXTENSION, ext);
      dialog.setArguments(args);
    }
    String tag =
        open ? OpenFileDialog.class.getName() : SaveFileDialog.class.getName();
    dialog.show(getSupportFragmentManager(), tag);
    try {
      mLatch.await();
    } catch (InterruptedException ex) {
      // do nothing.
    }
    return mFilename.get();
  }

  @Override
  public void onFileSelected(FileDialog dialog, File file) {
    mFilename.set(file.getAbsolutePath());
    // dismiss() is called internally to wrap up the UI flow.
  }

  public native void nativeInit(AssetManager assetManager);
}
