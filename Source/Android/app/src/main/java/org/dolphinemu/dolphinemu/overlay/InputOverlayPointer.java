package org.dolphinemu.dolphinemu.overlay;

import android.app.Activity;
import android.content.Context;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.MotionEvent;

public class InputOverlayPointer
{
  private final float[] axes = {0f, 0f};
  private float maxHeight;
  private float maxWidth;
  private int trackId = -1;

  public InputOverlayPointer(Context context)
  {
    Display display = ((Activity) context).getWindowManager().getDefaultDisplay();
    DisplayMetrics outMetrics = new DisplayMetrics();
    display.getMetrics(outMetrics);
    maxWidth = outMetrics.widthPixels / 2;
    maxHeight = outMetrics.heightPixels / 2;
  }

  public boolean onTouch(MotionEvent event)
  {
    int pointerIndex = event.getActionIndex();

    switch (event.getAction() & MotionEvent.ACTION_MASK)
    {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
        trackId = event.getPointerId(pointerIndex);
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
    axes[0] = (y - maxHeight) / maxHeight;
    axes[1] = (x - maxWidth) / maxWidth;

    return false;
  }

  public float[] getAxisValues()
  {
    float[] ir = {0f, 0f};
    ir[0] = axes[0];
    ir[1] = axes[1];
    return axes;
  }
}
