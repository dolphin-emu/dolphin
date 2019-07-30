/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.os.Handler;

import org.dolphinemu.dolphinemu.NativeLibrary;

public final class InputOverlayDrawableButton
{
  // The ID identifying what type of button this Drawable represents.
  private int mButtonId;
  private int mPointerId;
  private int mTiltStatus;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mControlPositionX, mControlPositionY;
  private BitmapDrawable mDefaultStateBitmap;
  private BitmapDrawable mPressedStateBitmap;
  private Handler mHandler;

  public InputOverlayDrawableButton(BitmapDrawable defaultBitmap, BitmapDrawable pressedBitmap, int buttonId)
  {
    mPointerId = -1;
    mTiltStatus = 0;
    mButtonId = buttonId;
    mDefaultStateBitmap = defaultBitmap;
    mPressedStateBitmap = pressedBitmap;

    // input hack
    if (mButtonId == InputOverlay.sInputHackForRK4)
    {
      mHandler = new Handler();
    }
  }

  public int getButtonId()
  {
    return mButtonId;
  }

  public int getPointerId()
  {
    return mPointerId;
  }

  public void onConfigureBegin(int x, int y)
  {
    mPreviousTouchX = x;
    mPreviousTouchY = y;
  }

  public void onConfigureMove(int x, int y)
  {
    Rect bounds = getBounds();
    mControlPositionX += x - mPreviousTouchX;
    mControlPositionY += y - mPreviousTouchY;
    setBounds(new Rect(mControlPositionX, mControlPositionY,
      mControlPositionX + bounds.width(), mControlPositionY + bounds.height()));
    mPreviousTouchX = x;
    mPreviousTouchY = y;
  }

  public void onDraw(Canvas canvas)
  {
    getCurrentStateBitmapDrawable().draw(canvas);
  }

  public void onPointerDown(int id, float x, float y)
  {
    mPointerId = id;
    if (mButtonId == NativeLibrary.ButtonType.WIIMOTE_TILT_TOGGLE)
    {
      float[] valueList = {0.5f, 1.0f, 0.0f};
      float value = valueList[mTiltStatus];
      mTiltStatus = (mTiltStatus + 1) % valueList.length;
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.WIIMOTE_TILT + 1, value);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.WIIMOTE_TILT + 2, value);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.WIIMOTE_TILT + 3, 0);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.WIIMOTE_TILT + 4, 0);
    }
    else
    {
      NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, mButtonId, NativeLibrary.ButtonState.PRESSED);
    }
  }

  public void onPointerMove(int id, float x, float y)
  {
  }

  public void onPointerUp(int id, float x, float y)
  {
    mPointerId = -1;
    if (mButtonId != NativeLibrary.ButtonType.WIIMOTE_TILT_TOGGLE)
    {
      NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, mButtonId, NativeLibrary.ButtonState.RELEASED);
      if (mButtonId == InputOverlay.sInputHackForRK4)
      {
        mHandler.postDelayed(() -> {
          NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.WIIMOTE_SHAKE_X + 2, 1);
          mHandler.postDelayed(() -> NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.WIIMOTE_SHAKE_X + 2, 0), 60);
          }, 120);
      }
    }
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  private BitmapDrawable getCurrentStateBitmapDrawable()
  {
    return mPointerId != -1 ? mPressedStateBitmap : mDefaultStateBitmap;
  }

  public void setBounds(Rect bounds)
  {
    mDefaultStateBitmap.setBounds(bounds);
    mPressedStateBitmap.setBounds(bounds);
  }

  public void setAlpha(int value)
  {
    mDefaultStateBitmap.setAlpha(value);
  }

  public Rect getBounds()
  {
    return mDefaultStateBitmap.getBounds();
  }
}
