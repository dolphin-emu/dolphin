/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;

import org.dolphinemu.dolphinemu.NativeLibrary;

public final class InputOverlayDrawableJoystick
{
  private final int[] mAxisIDs = {0, 0, 0, 0};
  private final float[] mAxises = {0f, 0f};
  private final float[] mFactors = {1, 1, 1, 1};

  private int mPointerId = -1;
  private int mJoystickType;
  private int mControlPositionX, mControlPositionY;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mAlpha;
  private Rect mVirtBounds;
  private Rect mOrigBounds;
  private BitmapDrawable mOuterBitmap;
  private BitmapDrawable mDefaultInnerBitmap;
  private BitmapDrawable mPressedInnerBitmap;
  private BitmapDrawable mBoundsBoxBitmap;

  public InputOverlayDrawableJoystick(BitmapDrawable bitmapBounds, BitmapDrawable bitmapOuter,
                                      BitmapDrawable InnerDefault, BitmapDrawable InnerPressed,
                                      Rect rectOuter, Rect rectInner, int joystick)
  {
    setAxisIDs(joystick);

    mOuterBitmap = bitmapOuter;
    mDefaultInnerBitmap = InnerDefault;
    mPressedInnerBitmap = InnerPressed;
    mBoundsBoxBitmap = bitmapBounds;

    setBounds(rectOuter);
    mDefaultInnerBitmap.setBounds(rectInner);
    mPressedInnerBitmap.setBounds(rectInner);
    mVirtBounds = mOuterBitmap.copyBounds();
    mOrigBounds = mOuterBitmap.copyBounds();
    mBoundsBoxBitmap.setAlpha(0);
    mBoundsBoxBitmap.setBounds(mVirtBounds);
    updateInnerBounds();
  }

  public int getButtonId()
  {
    return mJoystickType;
  }

  public void onDraw(Canvas canvas)
  {
    mOuterBitmap.draw(canvas);
    getCurrentBitmapDrawable().draw(canvas);
    mBoundsBoxBitmap.draw(canvas);
  }

  public void onPointerDown(int id, float x, float y)
  {
    boolean reCenter = InputOverlay.sJoystickRelative;
    mOuterBitmap.setAlpha(0);
    mBoundsBoxBitmap.setAlpha(mAlpha);
    if (reCenter)
    {
      mVirtBounds.offset((int)x - mVirtBounds.centerX(), (int)y - mVirtBounds.centerY());
    }
    mBoundsBoxBitmap.setBounds(mVirtBounds);
    mPointerId = id;

    setJoystickState(x, y);
  }

  public void onPointerMove(int id, float x, float y)
  {
    setJoystickState(x, y);
  }

  public void onPointerUp(int id, float x, float y)
  {
    mOuterBitmap.setAlpha(mAlpha);
    mBoundsBoxBitmap.setAlpha(0);
    mVirtBounds.set(mOrigBounds);
    setBounds(mOrigBounds);
    mPointerId = -1;

    setJoystickState(x, y);
  }

  public void setAxisIDs(int joystick)
  {
    if(joystick != 0)
    {
      mJoystickType = joystick;

      mFactors[0] = 1;
      mFactors[1] = 1;
      mFactors[2] = 1;
      mFactors[3] = 1;

      mAxisIDs[0] = joystick + 1;
      mAxisIDs[1] = joystick + 2;
      mAxisIDs[2] = joystick + 3;
      mAxisIDs[3] = joystick + 4;
      return;
    }

    switch(InputOverlay.sJoyStickSetting)
    {
      case InputOverlay.JOYSTICK_EMULATE_IR:
        mJoystickType = 0;

        mFactors[0] = 0.8f;
        mFactors[1] = 0.8f;
        mFactors[2] = 0.4f;
        mFactors[3] = 0.4f;

        mAxisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_IR + 1;
        mAxisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_IR + 2;
        mAxisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_IR + 3;
        mAxisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_IR + 4;
        break;
      case InputOverlay.JOYSTICK_EMULATE_WII_SWING:
        mJoystickType = 0;

        mFactors[0] = -0.8f;
        mFactors[1] = -0.8f;
        mFactors[2] = -0.8f;
        mFactors[3] = -0.8f;

        mAxisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_SWING + 1;
        mAxisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_SWING + 2;
        mAxisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_SWING + 3;
        mAxisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_SWING + 4;
        break;
      case InputOverlay.JOYSTICK_EMULATE_WII_TILT:
        mJoystickType = 0;
        if(InputOverlay.sControllerType == InputOverlay.CONTROLLER_WIINUNCHUK)
        {
          mFactors[0] = 0.8f;
          mFactors[1] = 0.8f;
          mFactors[2] = 0.8f;
          mFactors[3] = 0.8f;

          mAxisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_TILT + 1;
          mAxisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_TILT + 2;
          mAxisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_TILT + 3;
          mAxisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_TILT + 4;
        }
        else
        {
          mFactors[0] = -0.8f;
          mFactors[1] = -0.8f;
          mFactors[2] = 0.8f;
          mFactors[3] = 0.8f;

          mAxisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_TILT + 4; // right
          mAxisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_TILT + 3; // left
          mAxisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_TILT + 1; // up
          mAxisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_TILT + 2; // down
        }
        break;
      case InputOverlay.JOYSTICK_EMULATE_WII_SHAKE:
        mJoystickType = 0;
        mAxisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_SHAKE_X;
        mAxisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_SHAKE_X;
        mAxisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_SHAKE_Y;
        mAxisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_SHAKE_Z;
        break;
      case InputOverlay.JOYSTICK_EMULATE_NUNCHUK_SWING:
        mJoystickType = 0;

        mFactors[0] = -0.8f;
        mFactors[1] = -0.8f;
        mFactors[2] = -0.8f;
        mFactors[3] = -0.8f;

        mAxisIDs[0] = NativeLibrary.ButtonType.NUNCHUK_SWING + 1;
        mAxisIDs[1] = NativeLibrary.ButtonType.NUNCHUK_SWING + 2;
        mAxisIDs[2] = NativeLibrary.ButtonType.NUNCHUK_SWING + 3;
        mAxisIDs[3] = NativeLibrary.ButtonType.NUNCHUK_SWING + 4;
        break;
      case InputOverlay.JOYSTICK_EMULATE_NUNCHUK_TILT:
        mJoystickType = 0;

        mFactors[0] = 0.8f;
        mFactors[1] = 0.8f;
        mFactors[2] = 0.8f;
        mFactors[3] = 0.8f;

        mAxisIDs[0] = NativeLibrary.ButtonType.NUNCHUK_TILT + 1;
        mAxisIDs[1] = NativeLibrary.ButtonType.NUNCHUK_TILT + 2;
        mAxisIDs[2] = NativeLibrary.ButtonType.NUNCHUK_TILT + 3;
        mAxisIDs[3] = NativeLibrary.ButtonType.NUNCHUK_TILT + 4;
        break;
      case InputOverlay.JOYSTICK_EMULATE_NUNCHUK_SHAKE:
        mJoystickType = 0;
        mAxisIDs[0] = NativeLibrary.ButtonType.NUNCHUK_SHAKE_X;
        mAxisIDs[1] = NativeLibrary.ButtonType.NUNCHUK_SHAKE_X;
        mAxisIDs[2] = NativeLibrary.ButtonType.NUNCHUK_SHAKE_Y;
        mAxisIDs[3] = NativeLibrary.ButtonType.NUNCHUK_SHAKE_Z;
        break;
    }
  }

  private void setJoystickState(float touchX, float touchY)
  {
    if(mPointerId != -1)
    {
      float maxY = mVirtBounds.bottom;
      float maxX = mVirtBounds.right;
      touchX -= mVirtBounds.centerX();
      maxX -= mVirtBounds.centerX();
      touchY -= mVirtBounds.centerY();
      maxY -= mVirtBounds.centerY();
      final float AxisX = touchX / maxX;
      final float AxisY = touchY / maxY;
      mAxises[0] = AxisY;
      mAxises[1] = AxisX;
    }
    else
    {
      mAxises[0] = mAxises[1] = 0.0f;
    }

    updateInnerBounds();

    float[] axises = getAxisValues();

    if(mJoystickType != 0)
    {
      // fx wii classic or classic bind
      axises[1] = Math.min(axises[1], 1.0f);
      axises[0] = Math.min(axises[0], 0.0f);
      axises[3] = Math.min(axises[3], 1.0f);
      axises[2] = Math.min(axises[2], 0.0f);
    }
    else if (InputOverlay.sJoyStickSetting == InputOverlay.JOYSTICK_EMULATE_WII_SHAKE ||
      InputOverlay.sJoyStickSetting == InputOverlay.JOYSTICK_EMULATE_NUNCHUK_SHAKE)
    {
      // shake
      axises[0] = -axises[1];
      axises[1] = -axises[1];
      axises[3] = -axises[3];
      handleShakeEvent(axises);
      return;
    }

    for (int i = 0; i < 4; i++)
    {
      NativeLibrary.onGamePadMoveEvent(
        NativeLibrary.TouchScreenDevice, mAxisIDs[i], mFactors[i] * axises[i]);
    }
  }

  // axis to button
  private void handleShakeEvent(float[] axises)
  {
    for(int i = 0; i < axises.length; ++i)
    {
      if(axises[i] > 0.15f)
      {
        if(InputOverlay.sShakeStates[i] != NativeLibrary.ButtonState.PRESSED)
        {
          InputOverlay.sShakeStates[i] = NativeLibrary.ButtonState.PRESSED;
          NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, mAxisIDs[i],
            NativeLibrary.ButtonState.PRESSED);
        }
      }
      else if(InputOverlay.sShakeStates[i] != NativeLibrary.ButtonState.RELEASED)
      {
        InputOverlay.sShakeStates[i] = NativeLibrary.ButtonState.RELEASED;
        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, mAxisIDs[i],
          NativeLibrary.ButtonState.RELEASED);
      }
    }
  }

  public void onConfigureBegin(int x, int y)
  {
    mPreviousTouchX = x;
    mPreviousTouchY = y;
  }

  public void onConfigureMove(int x, int y)
  {
    int deltaX = x - mPreviousTouchX;
    int deltaY = y - mPreviousTouchY;
    Rect bounds = getBounds();
    mControlPositionX += deltaX;
    mControlPositionY += deltaY;
    setBounds(new Rect(mControlPositionX, mControlPositionY,
      mControlPositionX + bounds.width(),
      mControlPositionY + bounds.height()));
    mVirtBounds.set(mControlPositionX, mControlPositionY,
      mControlPositionX + mVirtBounds.width(),
      mControlPositionY + mVirtBounds.height());
    updateInnerBounds();
    mOrigBounds.set(mControlPositionX, mControlPositionY,
      mControlPositionX + mOrigBounds.width(),
      mControlPositionY + mOrigBounds.height());
    mPreviousTouchX = x;
    mPreviousTouchY = y;
  }

  public float[] getAxisValues()
  {
    return new float[]{mAxises[0], mAxises[0], mAxises[1], mAxises[1]};
  }

  private void updateInnerBounds()
  {
    int X = mVirtBounds.centerX() + (int) ((mAxises[1]) * (mVirtBounds.width() / 2));
    int Y = mVirtBounds.centerY() + (int) ((mAxises[0]) * (mVirtBounds.height() / 2));

    if (X > mVirtBounds.centerX() + (mVirtBounds.width() / 2))
      X = mVirtBounds.centerX() + (mVirtBounds.width() / 2);
    if (X < mVirtBounds.centerX() - (mVirtBounds.width() / 2))
      X = mVirtBounds.centerX() - (mVirtBounds.width() / 2);
    if (Y > mVirtBounds.centerY() + (mVirtBounds.height() / 2))
      Y = mVirtBounds.centerY() + (mVirtBounds.height() / 2);
    if (Y < mVirtBounds.centerY() - (mVirtBounds.height() / 2))
      Y = mVirtBounds.centerY() - (mVirtBounds.height() / 2);

    int width = mPressedInnerBitmap.getBounds().width() / 2;
    int height = mPressedInnerBitmap.getBounds().height() / 2;
    mDefaultInnerBitmap.setBounds(X - width, Y - height, X + width, Y + height);
    mPressedInnerBitmap.setBounds(mDefaultInnerBitmap.getBounds());
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  private BitmapDrawable getCurrentBitmapDrawable()
  {
    return mPointerId != -1 ? mPressedInnerBitmap : mDefaultInnerBitmap;
  }

  public void setBounds(Rect bounds)
  {
    mOuterBitmap.setBounds(bounds);
  }

  public void setAlpha(int value)
  {
    mAlpha = value;
    mDefaultInnerBitmap.setAlpha(value);
    mOuterBitmap.setAlpha(value);
  }

  public Rect getBounds()
  {
    return mOuterBitmap.getBounds();
  }

  public int getPointerId()
  {
    return mPointerId;
  }
}
