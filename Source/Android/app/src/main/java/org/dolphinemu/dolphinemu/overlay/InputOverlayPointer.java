// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.overlay;

import android.graphics.Rect;
import android.os.Handler;
import android.view.MotionEvent;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider;

import java.util.ArrayList;

public class InputOverlayPointer
{
  public static final int MODE_DISABLED = 0;
  public static final int MODE_FOLLOW = 1;
  public static final int MODE_DRAG = 2;

  private float mCurrentX = 0.0f;
  private float mCurrentY = 0.0f;
  private float mOldX = 0.0f;
  private float mOldY = 0.0f;

  private float mGameCenterX;
  private float mGameCenterY;
  private float mGameWidthHalfInv;
  private float mGameHeightHalfInv;

  private float mTouchStartX;
  private float mTouchStartY;

  private int mMode;
  private boolean mRecenter;
  private int mControllerIndex;

  private boolean doubleTap = false;
  private int mDoubleTapControl;
  private int trackId = -1;

  public static ArrayList<Integer> DOUBLE_TAP_OPTIONS = new ArrayList<>();

  static
  {
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_A);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_B);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.WIIMOTE_BUTTON_2);
    DOUBLE_TAP_OPTIONS.add(NativeLibrary.ButtonType.CLASSIC_BUTTON_A);
  }

  public InputOverlayPointer(Rect surfacePosition, int doubleTapControl, int mode, boolean recenter,
          int controllerIndex)
  {
    mDoubleTapControl = doubleTapControl;
    mMode = mode;
    mRecenter = recenter;
    mControllerIndex = controllerIndex;

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
    int action = event.getActionMasked();
    boolean firstPointer = action != MotionEvent.ACTION_POINTER_DOWN &&
            action != MotionEvent.ACTION_POINTER_UP;
    int pointerIndex = firstPointer ? 0 : event.getActionIndex();

    switch (action)
    {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
        trackId = event.getPointerId(pointerIndex);
        mTouchStartX = event.getX(pointerIndex);
        mTouchStartY = event.getY(pointerIndex);
        touchPress();
        break;
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_POINTER_UP:
        if (trackId == event.getPointerId(pointerIndex))
          trackId = -1;
        if (mMode == MODE_DRAG)
          updateOldAxes();
        if (mRecenter)
          reset();
        break;
    }

    int eventPointerIndex = event.findPointerIndex(trackId);
    if (trackId == -1 || eventPointerIndex == -1)
      return;

    if (mMode == MODE_FOLLOW)
    {
      mCurrentX = (event.getX(eventPointerIndex) - mGameCenterX) * mGameWidthHalfInv;
      mCurrentY = (event.getY(eventPointerIndex) - mGameCenterY) * mGameHeightHalfInv;
    }
    else if (mMode == MODE_DRAG)
    {
      mCurrentX = mOldX +
              (event.getX(eventPointerIndex) - mTouchStartX) * mGameWidthHalfInv;
      mCurrentY = mOldY +
              (event.getY(eventPointerIndex) - mTouchStartY) * mGameHeightHalfInv;
    }
  }

  private void touchPress()
  {
    if (mMode != MODE_DISABLED)
    {
      if (doubleTap)
      {
        InputOverrider.setControlState(mControllerIndex, mDoubleTapControl, 1.0);
        new Handler().postDelayed(() -> InputOverrider.setControlState(mControllerIndex,
                        mDoubleTapControl, 0.0),
                50);
      }
      else
      {
        doubleTap = true;
        new Handler().postDelayed(() -> doubleTap = false, 300);
      }
    }
  }

  private void updateOldAxes()
  {
    mOldX = mCurrentX;
    mOldY = mCurrentY;
  }

  private void reset()
  {
    mCurrentX = mCurrentY = mOldX = mOldY = 0.0f;
  }

  public float getX()
  {
    return mCurrentX;
  }

  public float getY()
  {
    return mCurrentY;
  }

  public void setMode(int mode)
  {
    mMode = mode;
    if (mode == MODE_DRAG)
      updateOldAxes();
  }

  public void setRecenter(boolean recenter)
  {
    mRecenter = recenter;
  }
}
