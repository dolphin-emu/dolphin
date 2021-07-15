// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.overlay;

import android.graphics.Rect;
import android.os.Handler;
import android.view.MotionEvent;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.features.input.InputOverrider;

import java.util.ArrayList;

public class InputOverlayPointer
{
  private float mCurrentX = 0.0f;
  private float mCurrentY = 0.0f;

  private float mGameCenterX;
  private float mGameCenterY;
  private float mGameWidthHalfInv;
  private float mGameHeightHalfInv;

  private boolean doubleTap = false;
  private int doubleTapControl;
  private int trackId = -1;

  public static ArrayList<Integer> DOUBLE_TAP_OPTIONS = new ArrayList<>();

  static
  {
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_A);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_B);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_2);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.CLASSIC_BUTTON_A);
  }

  public InputOverlayPointer(Rect surfacePosition, int doubleTapControl)
  {
    this.doubleTapControl = doubleTapControl;

    mGameCenterX = (surfacePosition.left + surfacePosition.right) / 2.0f;
    mGameCenterY = (surfacePosition.top + surfacePosition.bottom) / 2.0f;

    float gameWidth = surfacePosition.right - surfacePosition.left;
    float gameHeight = surfacePosition.bottom - surfacePosition.top;

    // Adjusting for device's black bars.
    float surfaceAR = gameWidth / gameHeight;
    float gameAR = NativeLibrary.GetGameAspectRatio();

    if (gameAR <= surfaceAR)
    {
      // Black bars on left/right
      gameWidth = gameHeight * gameAR;
    }
    else
    {
      // Black bars on top/bottom
      gameHeight = gameWidth / gameAR;
    }

    mGameWidthHalfInv = 1.0f / (gameWidth * 0.5f);
    mGameHeightHalfInv = 1.0f / (gameHeight * 0.5f);
  }

  public void onTouch(MotionEvent event)
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
      return;

    mCurrentX = (event.getX(event.findPointerIndex(trackId)) - mGameCenterX) * mGameWidthHalfInv;
    mCurrentY = (event.getY(event.findPointerIndex(trackId)) - mGameCenterY) * mGameHeightHalfInv;
  }

  private void touchPress()
  {
    if (doubleTap)
    {
      InputOverrider.setControlState(0, doubleTapControl, 1.0);
      new Handler().postDelayed(() -> InputOverrider.setControlState(0, doubleTapControl, 0.0), 50);
    }
    else
    {
      doubleTap = true;
      new Handler().postDelayed(() -> doubleTap = false, 300);
    }
  }

  public float getX()
  {
    return mCurrentX;
  }

  public float getY()
  {
    return mCurrentY;
  }
}
