/**
 * Copyright 2016 Dolphin Emulator Project
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
public final class InputOverlayDrawableDpad
{
  // The ID identifying what type of button this Drawable represents.
  private int[] mButtonType = new int[4];
  private boolean[] mPressStates = new boolean[4];
  private int mTrackId;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mControlPositionX, mControlPositionY;
  private int mWidth;
  private int mHeight;
  private BitmapDrawable mDefaultStateBitmap;
  private BitmapDrawable mPressedOneDirectionStateBitmap;
  private BitmapDrawable mPressedTwoDirectionsStateBitmap;

  /**
   * Constructor
   *
   * @param res                             {@link Resources} instance.
   * @param defaultStateBitmap              {@link Bitmap} of the default state.
   * @param pressedOneDirectionStateBitmap  {@link Bitmap} of the pressed state in one direction.
   * @param pressedTwoDirectionsStateBitmap {@link Bitmap} of the pressed state in two direction.
   * @param buttonUp                        Identifier for the up button.
   * @param buttonDown                      Identifier for the down button.
   * @param buttonLeft                      Identifier for the left button.
   * @param buttonRight                     Identifier for the right button.
   */
  public InputOverlayDrawableDpad(Resources res,
    Bitmap defaultStateBitmap,
    Bitmap pressedOneDirectionStateBitmap,
    Bitmap pressedTwoDirectionsStateBitmap,
    int buttonUp, int buttonDown,
    int buttonLeft, int buttonRight)
  {
    mDefaultStateBitmap = new BitmapDrawable(res, defaultStateBitmap);
    mPressedOneDirectionStateBitmap = new BitmapDrawable(res, pressedOneDirectionStateBitmap);
    mPressedTwoDirectionsStateBitmap = new BitmapDrawable(res, pressedTwoDirectionsStateBitmap);

    mWidth = mDefaultStateBitmap.getIntrinsicWidth();
    mHeight = mDefaultStateBitmap.getIntrinsicHeight();

    mButtonType[0] = buttonUp;
    mButtonType[1] = buttonDown;
    mButtonType[2] = buttonLeft;
    mButtonType[3] = buttonRight;

    mPressStates[0] = false;
    mPressStates[1] = false;
    mPressStates[2] = false;
    mPressStates[3] = false;

    mTrackId = -1;
  }

  public void onDraw(Canvas canvas)
  {
    int px = mControlPositionX + (mWidth / 2);
    int py = mControlPositionY + (mHeight / 2);

    boolean up = mPressStates[0];
    boolean down = mPressStates[1];
    boolean left = mPressStates[2];
    boolean right = mPressStates[3];

    if (up)
    {
      if (left)
        mPressedTwoDirectionsStateBitmap.draw(canvas);
      else if (right)
      {
        canvas.save();
        canvas.rotate(90, px, py);
        mPressedTwoDirectionsStateBitmap.draw(canvas);
        canvas.restore();
      }
      else
        mPressedOneDirectionStateBitmap.draw(canvas);
    }
    else if (down)
    {
      if (left)
      {
        canvas.save();
        canvas.rotate(270, px, py);
        mPressedTwoDirectionsStateBitmap.draw(canvas);
        canvas.restore();
      }
      else if (right)
      {
        canvas.save();
        canvas.rotate(180, px, py);
        mPressedTwoDirectionsStateBitmap.draw(canvas);
        canvas.restore();
      }
      else
      {
        canvas.save();
        canvas.rotate(180, px, py);
        mPressedOneDirectionStateBitmap.draw(canvas);
        canvas.restore();
      }
    }
    else if (left)
    {
      canvas.save();
      canvas.rotate(270, px, py);
      mPressedOneDirectionStateBitmap.draw(canvas);
      canvas.restore();
    }
    else if (right)
    {
      canvas.save();
      canvas.rotate(90, px, py);
      mPressedOneDirectionStateBitmap.draw(canvas);
      canvas.restore();
    }
    else
    {
      mDefaultStateBitmap.draw(canvas);
    }
  }

  /**
   * Gets one of the InputOverlayDrawableDpad's button IDs.
   *
   * @return the requested InputOverlayDrawableDpad's button ID.
   */
  public int getId(int direction)
  {
    return mButtonType[direction];
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
        setBounds(mControlPositionX, mControlPositionY, mWidth + mControlPositionX,
          mHeight + mControlPositionY);
        mPreviousTouchX = fingerPositionX;
        mPreviousTouchY = fingerPositionY;
        break;

    }
    return true;
  }

  public void onPointerDown(int id, float x, float y)
  {
    mTrackId = id;
    setDpadState((int)x, (int)y);
  }

  public void onPointerMove(int id, float x, float y)
  {
    setDpadState((int)x, (int)y);
  }

  public void onPointerUp(int id, float x, float y)
  {
    mTrackId = -1;
    setDpadState((int)x, (int)y);
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  public int getWidth()
  {
    return mWidth;
  }

  public int getHeight()
  {
    return mHeight;
  }

  public Rect getBounds()
  {
    return mDefaultStateBitmap.getBounds();
  }

  public void setBounds(int left, int top, int right, int bottom)
  {
    mDefaultStateBitmap.setBounds(left, top, right, bottom);
    mPressedOneDirectionStateBitmap.setBounds(left, top, right, bottom);
    mPressedTwoDirectionsStateBitmap.setBounds(left, top, right, bottom);
  }

  private void setDpadState(int pointerX, int pointerY)
  {
    // Up, Down, Left, Right
    boolean[] pressed = {false, false, false, false};

    if(mTrackId != -1)
    {
      Rect bounds = mDefaultStateBitmap.getBounds();

      if (bounds.top + (mHeight / 3) > pointerY)
        pressed[0] = true;
      if (bounds.bottom - (mHeight / 3) < pointerY)
        pressed[1] = true;
      if (bounds.left + (mWidth / 3) > pointerX)
        pressed[2] = true;
      if (bounds.right - (mWidth / 3) < pointerX)
        pressed[3] = true;
    }

    for(int i = 0; i < pressed.length; ++i)
    {
      if(pressed[i] != mPressStates[i])
      {
        NativeLibrary
          .onGamePadEvent(NativeLibrary.TouchScreenDevice, mButtonType[i],
            pressed[i] ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
      }
    }

    mPressStates[0] = pressed[0];
    mPressStates[1] = pressed[1];
    mPressStates[2] = pressed[2];
    mPressStates[3] = pressed[3];
  }
}
