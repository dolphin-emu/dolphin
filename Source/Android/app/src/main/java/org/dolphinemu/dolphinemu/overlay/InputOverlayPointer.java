package org.dolphinemu.dolphinemu.overlay;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.MotionEvent;

import org.dolphinemu.dolphinemu.NativeLibrary;

import java.util.ArrayList;

public class InputOverlayPointer
{
  public static final int DOUBLE_TAP_A = 0;
  public static final int DOUBLE_TAP_B = 1;
  public static final int DOUBLE_TAP_2 = 2;
  public static final int DOUBLE_TAP_CLASSIC_A = 3;

  private final float[] axes = {0f, 0f};

  private float maxHeight;
  private float maxWidth;
  private float aspectAdjusted;
  private boolean xAdjusted;
  private boolean doubleTap = false;
  private int doubleTapButton;
  private int trackId = -1;

  public static ArrayList<Integer> DOUBLE_TAP_OPTIONS = new ArrayList<>();

  static
  {
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_A);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_B);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_2);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.CLASSIC_BUTTON_A);
  }

  public InputOverlayPointer(Context context, int button)
  {
    Display display = ((Activity) context).getWindowManager().getDefaultDisplay();
    DisplayMetrics outMetrics = new DisplayMetrics();
    display.getMetrics(outMetrics);
    doubleTapButton = button;

    Integer y = outMetrics.heightPixels;
    Integer x = outMetrics.widthPixels;

    // Adjusting for device's black bars.
    Float deviceAR = (float) x / y;
    Float gameAR = NativeLibrary.GetGameAspectRatio();
    aspectAdjusted = gameAR / deviceAR;

    if (gameAR <= deviceAR) // Black bars on left/right
    {
      xAdjusted = true;
      Integer gameX = Math.round((float) y * gameAR);
      Integer buffer = (x - gameX);

      maxWidth = (float) (x - buffer) / 2;
      maxHeight = (float) y / 2;
    }
    else // Bars on top/bottom
    {
      xAdjusted = false;
      Integer gameY = Math.round((float) x / gameAR);
      Integer buffer = (y - gameY);

      maxWidth = (float) x / 2;
      maxHeight = (float) (y - buffer) / 2;
    }
  }

  public boolean onTouch(MotionEvent event)
  {
    int pointerIndex = event.getActionIndex();

    switch (event.getAction() & MotionEvent.ACTION_MASK)
    {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
        trackId = event.getPointerId(pointerIndex);
        touchPress();
        break;
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_POINTER_UP:
        if (trackId == event.getPointerId(pointerIndex))
          trackId = -1;
        break;
    }

    if (trackId == -1)
      return false;

    int x = (int) event.getX(event.findPointerIndex(trackId));
    int y = (int) event.getY(event.findPointerIndex(trackId));
    if (xAdjusted)
    {
      axes[0] = (y - maxHeight) / maxHeight;
      axes[1] = ((x * aspectAdjusted) - maxWidth) / maxWidth;
    }
    else
    {
      axes[0] = ((y * aspectAdjusted) - maxHeight) / maxHeight;
      axes[1] = (x - maxWidth) / maxWidth;
    }
    return false;
  }

  private void touchPress()
  {
    if (doubleTap)
    {
      NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice,
              doubleTapButton, NativeLibrary.ButtonState.PRESSED);
      new Handler().postDelayed(() -> NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice,
              doubleTapButton, NativeLibrary.ButtonState.RELEASED), 50);
    }
    else
    {
      doubleTap = true;
      new Handler().postDelayed(() -> doubleTap = false, 300);
    }
  }

  public float[] getAxisValues()
  {
    float[] iraxes = {0f, 0f, 0f, 0f};
    iraxes[1] = axes[0];
    iraxes[0] = axes[0];
    iraxes[3] = axes[1];
    iraxes[2] = axes[1];
    return iraxes;
  }
}
