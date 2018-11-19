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
public final class InputOverlayDrawableButton
{
  // The ID identifying what type of button this Drawable represents.
  private int mButtonType;
  private int mTrackId;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mControlPositionX, mControlPositionY;
  private int mWidth;
  private int mHeight;
  private BitmapDrawable mDefaultStateBitmap;
  private BitmapDrawable mPressedStateBitmap;

  /**
   * Constructor
   *
   * @param res                {@link Resources} instance.
   * @param defaultStateBitmap {@link Bitmap} to use with the default state Drawable.
   * @param pressedStateBitmap {@link Bitmap} to use with the pressed state Drawable.
   * @param buttonType         Identifier for this type of button.
   */
  public InputOverlayDrawableButton(Resources res, Bitmap defaultStateBitmap,
    Bitmap pressedStateBitmap, int buttonType)
  {
    mDefaultStateBitmap = new BitmapDrawable(res, defaultStateBitmap);
    mPressedStateBitmap = new BitmapDrawable(res, pressedStateBitmap);
    mButtonType = buttonType;

    mWidth = mDefaultStateBitmap.getIntrinsicWidth();
    mHeight = mDefaultStateBitmap.getIntrinsicHeight();

    mTrackId = -1;
  }

  /**
   * Gets this InputOverlayDrawableButton's button ID.
   *
   * @return this InputOverlayDrawableButton's button ID.
   */
  public int getId()
  {
    return mButtonType;
  }

  public int getTrackId()
  {
    return mTrackId;
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
        mControlPositionX += fingerPositionX - mPreviousTouchX;
        mControlPositionY += fingerPositionY - mPreviousTouchY;
        setBounds(mControlPositionX, mControlPositionY, getWidth() + mControlPositionX,
          getHeight() + mControlPositionY);
        mPreviousTouchX = fingerPositionX;
        mPreviousTouchY = fingerPositionY;
        break;

    }
    return true;
  }

  public void onDraw(Canvas canvas)
  {
    getCurrentStateBitmapDrawable().draw(canvas);
  }

  public void onPointerDown(int id, float x, float y)
  {
    mTrackId = id;
    NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, mButtonType, NativeLibrary.ButtonState.PRESSED);
  }

  public void onPointerMove(int id, float x, float y)
  {
  }

  public void onPointerUp(int id, float x, float y)
  {
    mTrackId = -1;
    NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, mButtonType, NativeLibrary.ButtonState.RELEASED);
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  private BitmapDrawable getCurrentStateBitmapDrawable()
  {
    return mTrackId != -1 ? mPressedStateBitmap : mDefaultStateBitmap;
  }

  public void setBounds(int left, int top, int right, int bottom)
  {
    mDefaultStateBitmap.setBounds(left, top, right, bottom);
    mPressedStateBitmap.setBounds(left, top, right, bottom);
  }

  public Rect getBounds()
  {
    return mDefaultStateBitmap.getBounds();
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
