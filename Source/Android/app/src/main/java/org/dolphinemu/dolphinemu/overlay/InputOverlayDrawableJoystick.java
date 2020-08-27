/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

import android.content.SharedPreferences;
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
  private SharedPreferences mPreferences;

  private final int[] axisIDs = {0, 0, 0, 0};
  private final float[] axises = {0f, 0f};
  private int trackId = -1;
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
  private boolean mPressedState = false;

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
          Rect rectOuter, Rect rectInner, int joystick, SharedPreferences prefsHandle)
  {
    axisIDs[0] = joystick + 1;
    axisIDs[1] = joystick + 2;
    axisIDs[2] = joystick + 3;
    axisIDs[3] = joystick + 4;
    mJoystickType = joystick;

    mPreferences = prefsHandle;
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

  public void draw(Canvas canvas)
  {
    mOuterBitmap.draw(canvas);
    getCurrentStateBitmapDrawable().draw(canvas);
    mBoundsBoxBitmap.draw(canvas);
  }

  public boolean TrackEvent(MotionEvent event)
  {
    boolean reCenter = mPreferences.getBoolean("joystickRelCenter", true);
    int pointerIndex = event.getActionIndex();
    boolean pressed = false;

    switch (event.getAction() & MotionEvent.ACTION_MASK)
    {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
        if (getBounds().contains((int) event.getX(pointerIndex), (int) event.getY(pointerIndex)))
        {
          mPressedState = pressed = true;
          mOuterBitmap.setAlpha(0);
          mBoundsBoxBitmap.setAlpha(255);
          if (reCenter)
          {
            getVirtBounds().offset((int) event.getX(pointerIndex) - getVirtBounds().centerX(),
                    (int) event.getY(pointerIndex) - getVirtBounds().centerY());
          }
          mBoundsBoxBitmap.setBounds(getVirtBounds());
          trackId = event.getPointerId(pointerIndex);
        }
        break;
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_POINTER_UP:
        if (trackId == event.getPointerId(pointerIndex))
        {
          pressed = true;
          mPressedState = false;
          axises[0] = axises[1] = 0.0f;
          mOuterBitmap.setAlpha(255);
          mBoundsBoxBitmap.setAlpha(0);
          setVirtBounds(new Rect(mOrigBounds.left, mOrigBounds.top, mOrigBounds.right,
                  mOrigBounds.bottom));
          setBounds(new Rect(mOrigBounds.left, mOrigBounds.top, mOrigBounds.right,
                  mOrigBounds.bottom));
          SetInnerBounds();
          trackId = -1;
        }
        break;
    }

    if (trackId == -1)
      return pressed;

    for (int i = 0; i < event.getPointerCount(); i++)
    {
      if (trackId == event.getPointerId(i))
      {
        float touchX = event.getX(i);
        float touchY = event.getY(i);
        float maxY = getVirtBounds().bottom;
        float maxX = getVirtBounds().right;
        touchX -= getVirtBounds().centerX();
        maxX -= getVirtBounds().centerX();
        touchY -= getVirtBounds().centerY();
        maxY -= getVirtBounds().centerY();
        final float AxisX = touchX / maxX;
        final float AxisY = touchY / maxY;
        axises[0] = AxisY;
        axises[1] = AxisX;

        SetInnerBounds();
      }
    }
    return pressed;
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
    float[] joyaxises = {0f, 0f, 0f, 0f};
    joyaxises[1] = Math.min(axises[0], 1.0f);
    joyaxises[0] = Math.min(axises[0], 0.0f);
    joyaxises[3] = Math.min(axises[1], 1.0f);
    joyaxises[2] = Math.min(axises[1], 0.0f);
    return joyaxises;
  }

  public int[] getAxisIDs()
  {
    return axisIDs;
  }

  private void SetInnerBounds()
  {
    double y = axises[0];
    double x = axises[1];

    double angle = Math.atan2(y, x) + Math.PI + Math.PI;
    double radius = Math.hypot(y, x);
    double maxRadius = NativeLibrary.GetInputRadiusAtAngle(0, mJoystickType, angle);
    if (radius > maxRadius)
    {
      y = maxRadius * Math.sin(angle);
      x = maxRadius * Math.cos(angle);
      axises[0] = (float) y;
      axises[1] = (float) x;
    }

    int pixelX = getVirtBounds().centerX() + (int) (x * (getVirtBounds().width() / 2));
    int pixelY = getVirtBounds().centerY() + (int) (y * (getVirtBounds().height() / 2));

    int width = mPressedStateInnerBitmap.getBounds().width() / 2;
    int height = mPressedStateInnerBitmap.getBounds().height() / 2;
    mDefaultStateInnerBitmap.setBounds(pixelX - width, pixelY - height, pixelX + width,
            pixelY + height);
    mPressedStateInnerBitmap.setBounds(mDefaultStateInnerBitmap.getBounds());
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  private BitmapDrawable getCurrentStateBitmapDrawable()
  {
    return mPressedState ? mPressedStateInnerBitmap : mDefaultStateInnerBitmap;
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

  public int getWidth()
  {
    return mWidth;
  }

  public int getHeight()
  {
    return mHeight;
  }

  public int getTrackId()
  {
    return trackId;
  }
}
