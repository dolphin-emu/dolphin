/*
 * Copyright 2021 Dolphin Emulator
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
import org.dolphinemu.dolphinemu.NativeLibrary.Hotkey;

/**
 * Custom {@link BitmapDrawable} capable of storing
 * a setting to toggle when pressed
 */
public final class InputOverlayDrawableHotkey
{
  // The ID identifying the hotkey this Drawable represents.
  private int mHotkeyId;
  private int mButtonType;
  private int mTrackId;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mControlPositionX, mControlPositionY;
  private int mWidth;
  private int mHeight;
  private BitmapDrawable mEnabledStateBitmap;
  private BitmapDrawable mDisabledStateBitmap;
  private BitmapDrawable mPressedStateBitmap;
  private boolean mPressedState = false;
  private boolean mEnabledState;

  /**
   * Constructor
   *
   * @param res                 {@link Resources} instance.
   * @param enabledStateBitmap  {@link Bitmap} to use with the default state Drawable.
   * @param disabledStateBitmap {@link Bitmap} to use with the default state Drawable.
   * @param pressedStateBitmap  {@link Bitmap} to use with the pressed state Drawable.
   * @param hotkeyId            The hotkey ID associated to this drawable.
   */
  public InputOverlayDrawableHotkey(Resources res, Bitmap enabledStateBitmap,
          Bitmap disabledStateBitmap,
          Bitmap pressedStateBitmap, int buttonType, int hotkeyId)
  {
    mTrackId = -1;
    mEnabledStateBitmap = new BitmapDrawable(res, enabledStateBitmap);
    mDisabledStateBitmap = new BitmapDrawable(res, disabledStateBitmap);
    mPressedStateBitmap = new BitmapDrawable(res, pressedStateBitmap);
    mHotkeyId = hotkeyId;
    mButtonType = buttonType;
    mEnabledState = NativeLibrary.getHotkeyState(mHotkeyId);

    mWidth = mDisabledStateBitmap.getIntrinsicWidth();
    mHeight = mDisabledStateBitmap.getIntrinsicHeight();
  }

  /**
   * Gets this InputOverlayDrawableHotkey's button ID.
   *
   * @return this InputOverlayDrawableHotkey's button ID.
   */
  public int getId()
  {
    return mButtonType;
  }

  /**
   * Gets this InputOverlayDrawableHotkey's hotkey ID (mode).
   *
   * @return this InputOverlayDrawableHotkey's hotkey ID.
   */
  public int getHotkeyId()
  {
    return mHotkeyId;
  }

  public void setTrackId(int trackId)
  {
    mTrackId = trackId;
  }

  public int getTrackId()
  {
    return mTrackId;
  }

  public void onConfigureTouch(MotionEvent event)
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
        mControlPositionX += fingerPositionX - mPreviousTouchX;
        mControlPositionY += fingerPositionY - mPreviousTouchY;
        setBounds(mControlPositionX, mControlPositionY, getWidth() + mControlPositionX,
                getHeight() + mControlPositionY);
        mPreviousTouchX = fingerPositionX;
        mPreviousTouchY = fingerPositionY;
        break;
    }
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  public void draw(Canvas canvas)
  {
    getCurrentStateBitmapDrawable().draw(canvas);
  }

  private BitmapDrawable getCurrentStateBitmapDrawable()
  {
    return mPressedState ? mPressedStateBitmap :
            mEnabledState ? mEnabledStateBitmap : mDisabledStateBitmap;
  }

  public void setBounds(int left, int top, int right, int bottom)
  {
    mEnabledStateBitmap.setBounds(left, top, right, bottom);
    mDisabledStateBitmap.setBounds(left, top, right, bottom);
    mPressedStateBitmap.setBounds(left, top, right, bottom);
  }

  public void setOpacity(int value)
  {
    mEnabledStateBitmap.setAlpha(value);
    mDisabledStateBitmap.setAlpha(value);
  }

  public Rect getBounds()
  {
    return mDisabledStateBitmap.getBounds();
  }

  public int getWidth()
  {
    return mWidth;
  }

  public int getHeight()
  {
    return mHeight;
  }

  public void setPressedState(boolean isPressed)
  {
    mPressedState = isPressed;
  }

  public void setEnabledState(boolean isEnabled)
  {
    mEnabledState = isEnabled;
  }
}
