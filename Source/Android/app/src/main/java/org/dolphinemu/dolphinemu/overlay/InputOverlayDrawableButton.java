/*
 * Copyright 2013 Dolphin Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

package org.dolphinemu.dolphinemu.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
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
  private boolean mPressedState = false;

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
    mTrackId = -1;
    mDefaultStateBitmap = new BitmapDrawable(res, defaultStateBitmap);
    mPressedStateBitmap = new BitmapDrawable(res, pressedStateBitmap);
    mButtonType = buttonType;

    //Resizing Drawable for Guitar Overlay Button
    boolean isPortrait = res.getConfiguration().orientation == 1;
    int pWidth = mDefaultStateBitmap.getIntrinsicHeight();
    int pHeight = mDefaultStateBitmap.getIntrinsicWidth();
    int nWidth = (int) (mDefaultStateBitmap.getIntrinsicWidth() -
            (mDefaultStateBitmap.getIntrinsicWidth() / 3.0f));
    int nHeight = (int) (mDefaultStateBitmap.getIntrinsicHeight() +
            (mDefaultStateBitmap.getIntrinsicHeight() / 1.5f));

    if (isPortrait)
    {
      if (mButtonType == NativeLibrary.ButtonType.GUITAR_FRET_GREEN ||
              mButtonType == NativeLibrary.ButtonType.GUITAR_FRET_BLUE)
      {
        mDefaultStateBitmap = RotateBitmapDrawable(res, defaultStateBitmap, 90);
        mPressedStateBitmap = RotateBitmapDrawable(res, pressedStateBitmap, 90);
      }

      if (mButtonType == NativeLibrary.ButtonType.GUITAR_FRET_RED ||
              mButtonType == NativeLibrary.ButtonType.GUITAR_FRET_ORANGE ||
              mButtonType == NativeLibrary.ButtonType.GUITAR_STRUM_UP ||
              mButtonType == NativeLibrary.ButtonType.GUITAR_STRUM_DOWN)
      {
        mDefaultStateBitmap = RotateBitmapDrawable(res, defaultStateBitmap, -90);
        mPressedStateBitmap = RotateBitmapDrawable(res, pressedStateBitmap, -90);
      }

      if (mButtonType == NativeLibrary.ButtonType.GUITAR_FRET_YELLOW)
      {
        mDefaultStateBitmap = RotateBitmapDrawable(res, defaultStateBitmap, 180);
        mPressedStateBitmap = RotateBitmapDrawable(res, pressedStateBitmap, 180);
      }

      if (mButtonType >= NativeLibrary.ButtonType.GUITAR_FRET_GREEN &&
              mButtonType <= NativeLibrary.ButtonType.GUITAR_STRUM_DOWN)
      {
        if (mButtonType == NativeLibrary.ButtonType.GUITAR_FRET_YELLOW)
        {
          mWidth = (int) (nHeight / 2.5f);
        }
        else
        {
          mWidth = (int) (nHeight / 2f);
        }
        mHeight = nWidth;
      }
      else
      {
        mWidth = pWidth;
        mHeight = pHeight;
      }
    }
    // Landscape
    else
    {
      if (mButtonType >= NativeLibrary.ButtonType.GUITAR_FRET_GREEN &&
              mButtonType <= NativeLibrary.ButtonType.GUITAR_FRET_ORANGE)
      {
        mWidth = nWidth;
        mHeight = nHeight;
      }
      else if (mButtonType == NativeLibrary.ButtonType.GUITAR_STRUM_UP ||
              mButtonType == NativeLibrary.ButtonType.GUITAR_STRUM_DOWN)
      {
        mWidth = (int) (nWidth * 1.5f);
        mHeight = (int) (nHeight / 1.5f);
      }
      else
      {
        mWidth = pWidth;
        mHeight = pHeight;
      }
    }
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
    return mPressedState ? mPressedStateBitmap : mDefaultStateBitmap;
  }

  public void setBounds(int left, int top, int right, int bottom)
  {
    mDefaultStateBitmap.setBounds(left, top, right, bottom);
    mPressedStateBitmap.setBounds(left, top, right, bottom);
  }

  public void setOpacity(int value)
  {
    mDefaultStateBitmap.setAlpha(value);
    mPressedStateBitmap.setAlpha(value);
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

  public void setPressedState(boolean isPressed)
  {
    mPressedState = isPressed;
  }

  private BitmapDrawable RotateBitmapDrawable(Resources res, Bitmap bitmap, float degrees)
  {

    Matrix matrix = new Matrix();
    matrix.setRotate(degrees);

    Bitmap RotatedBitmap =
            Bitmap.createBitmap(bitmap, 0, 0, bitmap.getWidth(), bitmap.getHeight(), matrix, true);
    BitmapDrawable bitmapDrawable = new BitmapDrawable(res, RotatedBitmap);
    return bitmapDrawable;
  }
}
