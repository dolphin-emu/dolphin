/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.view.MotionEvent;

import org.dolphinemu.dolphinemu.NativeLibrary;

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableJoystick
{
  private final int[] mAxisIDs = {0, 0, 0, 0};
  private final float[] mAxises = {0f, 0f};
  private int mTrackId = -1;
  private int mJoystickType;
  private int mControlPositionX, mControlPositionY;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mWidth;
  private int mHeight;
  private Rect mVirtBounds;
  private Rect mOrigBounds;
  private BitmapDrawable mOuterBitmap;
  private BitmapDrawable mDefaultStateInnerBitmap;
  private BitmapDrawable mPressedStateInnerBitmap;
  private BitmapDrawable mBoundsBoxBitmap;

  /**
   * Constructor
   *
   * @param res                {@link Resources} instance.
   * @param bitmapOuter        {@link Bitmap} which represents the outer non-movable part of the joystick.
   * @param bitmapInnerDefault {@link Bitmap} which represents the default inner movable part of the joystick.
   * @param bitmapInnerPressed {@link Bitmap} which represents the pressed inner movable part of the joystick.
   * @param rectOuter          {@link Rect} which represents the outer joystick bounds.
   * @param rectInner          {@link Rect} which represents the inner joystick bounds.
   * @param joystick           Identifier for which joystick this is.
   */
  public InputOverlayDrawableJoystick(Resources res, Bitmap bitmapOuter,
    Bitmap bitmapInnerDefault, Bitmap bitmapInnerPressed,
    Rect rectOuter, Rect rectInner, int joystick)
  {
    mAxisIDs[0] = joystick + 1;
    mAxisIDs[1] = joystick + 2;
    mAxisIDs[2] = joystick + 3;
    mAxisIDs[3] = joystick + 4;
    mJoystickType = joystick;

    mOuterBitmap = new BitmapDrawable(res, bitmapOuter);
    mDefaultStateInnerBitmap = new BitmapDrawable(res, bitmapInnerDefault);
    mPressedStateInnerBitmap = new BitmapDrawable(res, bitmapInnerPressed);
    mBoundsBoxBitmap = new BitmapDrawable(res, bitmapOuter);
    mWidth = bitmapOuter.getWidth();
    mHeight = bitmapOuter.getHeight();

    setBounds(rectOuter);
    mDefaultStateInnerBitmap.setBounds(rectInner);
    mPressedStateInnerBitmap.setBounds(rectInner);
    mVirtBounds = getBounds();
    mOrigBounds = mOuterBitmap.copyBounds();
    mBoundsBoxBitmap.setAlpha(0);
    mBoundsBoxBitmap.setBounds(getVirtBounds());
    SetInnerBounds();
  }

  /**
   * Gets this InputOverlayDrawableJoystick's button ID.
   *
   * @return this InputOverlayDrawableJoystick's button ID.
   */
  public int getId()
  {
    return mJoystickType;
  }

  public void onDraw(Canvas canvas)
  {
    mOuterBitmap.draw(canvas);
    getCurrentStateBitmapDrawable().draw(canvas);
    mBoundsBoxBitmap.draw(canvas);
  }

  public void onPointerDown(int id, float x, float y)
  {
    boolean reCenter = InputOverlay.sJoystickRelative;
    mOuterBitmap.setAlpha(0);
    mBoundsBoxBitmap.setAlpha(255);
    if (reCenter)
    {
      getVirtBounds().offset((int)x - getVirtBounds().centerX(), (int)y - getVirtBounds().centerY());
    }
    mBoundsBoxBitmap.setBounds(getVirtBounds());
    mTrackId = id;

    setJoystickState(x, y);
  }

  public void onPointerMove(int id, float x, float y)
  {
    setJoystickState(x, y);
  }

  public void onPointerUp(int id, float x, float y)
  {
    mOuterBitmap.setAlpha(255);
    mBoundsBoxBitmap.setAlpha(0);
    setVirtBounds(new Rect(mOrigBounds.left, mOrigBounds.top, mOrigBounds.right, mOrigBounds.bottom));
    setBounds(new Rect(mOrigBounds.left, mOrigBounds.top, mOrigBounds.right, mOrigBounds.bottom));
    mTrackId = -1;

    setJoystickState(x, y);
  }

  private void setJoystickState(float touchX, float touchY)
  {
    if(mTrackId != -1)
    {
      float maxY = getVirtBounds().bottom;
      float maxX = getVirtBounds().right;
      touchX -= getVirtBounds().centerX();
      maxX -= getVirtBounds().centerX();
      touchY -= getVirtBounds().centerY();
      maxY -= getVirtBounds().centerY();
      final float AxisX = touchX / maxX;
      final float AxisY = touchY / maxY;
      mAxises[0] = AxisY;
      mAxises[1] = AxisX;
    }
    else
    {
      mAxises[0] = mAxises[1] = 0.0f;
    }

    SetInnerBounds();

    float factor = 1.0f;
    int[] axisIDs = getAxisIDs();
    float[] axises = getAxisValues();

    // fx wii classic
    if(axisIDs[0] == NativeLibrary.ButtonType.CLASSIC_STICK_LEFT_UP && axises[0] > 0)
    {
      axises[1] = 0;
    }

    if (axisIDs[0] == NativeLibrary.ButtonType.NUNCHUK_STICK_UP ||
      (InputOverlay.sControllerType == InputOverlay.COCONTROLLER_CLASSIC &&
        axisIDs[0] == NativeLibrary.ButtonType.CLASSIC_STICK_LEFT_UP))
    {
      if (InputOverlay.sJoyStickSetting == InputOverlay.JOYSTICK_EMULATE_IR)
      {
        factor = -1.0f;
        axisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_IR + 1;
        axisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_IR + 2;
        axisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_IR + 3;
        axisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_IR + 4;
      }
      else if (InputOverlay.sJoyStickSetting == InputOverlay.JOYSTICK_EMULATE_SWING)
      {
        factor = -1.0f;
        axisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_SWING + 1;
        axisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_SWING + 2;
        axisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_SWING + 3;
        axisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_SWING + 4;
      }
      else if (InputOverlay.sJoyStickSetting == InputOverlay.JOYSTICK_EMULATE_TILT)
      {
        if(InputOverlay.sControllerType == InputOverlay.CONTROLLER_WIINUNCHUK)
        {
          axisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_TILT + 1;
          axisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_TILT + 2;
          axisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_TILT + 3;
          axisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_TILT + 4;
        }
        else
        {
          axisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_TILT + 4;
          axisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_TILT + 3;
          axisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_TILT + 1;
          axisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_TILT + 2;
        }
      }
      else if (InputOverlay.sJoyStickSetting == InputOverlay.JOYSTICK_EMULATE_SHAKE)
      {
        axises[1] = -axises[1];
        axises[3] = -axises[3];
        handleShakeEvent(axises);
        return;
      }
    }

    for (int i = 0; i < 4; i++)
    {
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, axisIDs[i], factor * axises[i]);
    }
  }

  // axis to button
  private int[] mShakeState = new int[4];
  private void handleShakeEvent(float[] axises)
  {
    int[] axisIDs = new int[4];
    axisIDs[0] = 0;
    axisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_SHAKE_X;
    axisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_SHAKE_Y;
    axisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_SHAKE_Z;

    for(int i = 0; i < axises.length; ++i)
    {
      if(axises[i] > 0.15f)
      {
        if(mShakeState[i] != NativeLibrary.ButtonState.PRESSED)
        {
          mShakeState[i] = NativeLibrary.ButtonState.PRESSED;
          NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, axisIDs[i], mShakeState[i]);
        }
      }
      else if(mShakeState[i] != NativeLibrary.ButtonState.RELEASED)
      {
        mShakeState[i] = NativeLibrary.ButtonState.RELEASED;
        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, axisIDs[i], mShakeState[i]);
      }
    }
  }

  public boolean onConfigureTouch(MotionEvent event)
  {
    int pointerIndex = event.getActionIndex();
    int fingerPositionX = (int) event.getX(pointerIndex);
    int fingerPositionY = (int) event.getY(pointerIndex);
    switch (event.getAction())
    {
      case MotionEvent.ACTION_DOWN:
        mPreviousTouchX = fingerPositionX;
        mPreviousTouchY = fingerPositionY;
        break;
      case MotionEvent.ACTION_MOVE:
        int deltaX = fingerPositionX - mPreviousTouchX;
        int deltaY = fingerPositionY - mPreviousTouchY;
        mControlPositionX += deltaX;
        mControlPositionY += deltaY;
        setBounds(new Rect(mControlPositionX, mControlPositionY,
          mOuterBitmap.getIntrinsicWidth() + mControlPositionX,
          mOuterBitmap.getIntrinsicHeight() + mControlPositionY));
        setVirtBounds(new Rect(mControlPositionX, mControlPositionY,
          mOuterBitmap.getIntrinsicWidth() + mControlPositionX,
          mOuterBitmap.getIntrinsicHeight() + mControlPositionY));
        SetInnerBounds();
        setOrigBounds(new Rect(new Rect(mControlPositionX, mControlPositionY,
          mOuterBitmap.getIntrinsicWidth() + mControlPositionX,
          mOuterBitmap.getIntrinsicHeight() + mControlPositionY)));
        mPreviousTouchX = fingerPositionX;
        mPreviousTouchY = fingerPositionY;
        break;
    }
    return true;
  }

  public float[] getAxisValues()
  {
    return new float[]{mAxises[0], mAxises[0], mAxises[1], mAxises[1]};
  }

  public int[] getAxisIDs()
  {
    return new int[]{mAxisIDs[0], mAxisIDs[1], mAxisIDs[2], mAxisIDs[3]};
  }

  private void SetInnerBounds()
  {
    int X = getVirtBounds().centerX() + (int) ((mAxises[1]) * (getVirtBounds().width() / 2));
    int Y = getVirtBounds().centerY() + (int) ((mAxises[0]) * (getVirtBounds().height() / 2));

    if (X > getVirtBounds().centerX() + (getVirtBounds().width() / 2))
      X = getVirtBounds().centerX() + (getVirtBounds().width() / 2);
    if (X < getVirtBounds().centerX() - (getVirtBounds().width() / 2))
      X = getVirtBounds().centerX() - (getVirtBounds().width() / 2);
    if (Y > getVirtBounds().centerY() + (getVirtBounds().height() / 2))
      Y = getVirtBounds().centerY() + (getVirtBounds().height() / 2);
    if (Y < getVirtBounds().centerY() - (getVirtBounds().height() / 2))
      Y = getVirtBounds().centerY() - (getVirtBounds().height() / 2);

    int width = mPressedStateInnerBitmap.getBounds().width() / 2;
    int height = mPressedStateInnerBitmap.getBounds().height() / 2;
    mDefaultStateInnerBitmap.setBounds(X - width, Y - height, X + width, Y + height);
    mPressedStateInnerBitmap.setBounds(mDefaultStateInnerBitmap.getBounds());
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  private BitmapDrawable getCurrentStateBitmapDrawable()
  {
    return mTrackId != -1 ? mPressedStateInnerBitmap : mDefaultStateInnerBitmap;
  }

  public void setBounds(Rect bounds)
  {
    mOuterBitmap.setBounds(bounds);
  }

  public Rect getBounds()
  {
    return mOuterBitmap.getBounds();
  }

  private void setVirtBounds(Rect bounds)
  {
    mVirtBounds = bounds;
  }

  private void setOrigBounds(Rect bounds)
  {
    mOrigBounds = bounds;
  }

  private Rect getVirtBounds()
  {
    return mVirtBounds;
  }

  public int getTrackId()
  {
    return mTrackId;
  }

  public int getWidth()
  {
    return mWidth;
  }

  public int getHeight()
  {
    return mHeight;
  }
}
