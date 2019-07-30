/**
 * Copyright 2016 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;

import org.dolphinemu.dolphinemu.NativeLibrary;

public final class InputOverlayDrawableDpad
{
  // The ID identifying what type of button this Drawable represents.
  private int[] mButtonIds = new int[4];
  private boolean[] mPressStates = new boolean[4];
  private int mPointerId;
  private int mPreviousTouchX, mPreviousTouchY;
  private int mControlPositionX, mControlPositionY;
  private BitmapDrawable mDefaultStateBitmap;
  private BitmapDrawable mPressedOneDirectionStateBitmap;
  private BitmapDrawable mPressedTwoDirectionsStateBitmap;

  public InputOverlayDrawableDpad(BitmapDrawable defaultBitmap, BitmapDrawable onePressedBitmap,
                                  BitmapDrawable twoPressedBitmap,
                                  int buttonUp, int buttonDown, int buttonLeft, int buttonRight)
  {
    mPointerId = -1;
    mDefaultStateBitmap = defaultBitmap;
    mPressedOneDirectionStateBitmap = onePressedBitmap;
    mPressedTwoDirectionsStateBitmap = twoPressedBitmap;

    mButtonIds[0] = buttonUp;
    mButtonIds[1] = buttonDown;
    mButtonIds[2] = buttonLeft;
    mButtonIds[3] = buttonRight;

    mPressStates[0] = false;
    mPressStates[1] = false;
    mPressStates[2] = false;
    mPressStates[3] = false;
  }

  public void onDraw(Canvas canvas)
  {
    Rect bounds = getBounds();
    int px = mControlPositionX + (bounds.width() / 2);
    int py = mControlPositionY + (bounds.height() / 2);

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

  public int getButtonId(int direction)
  {
    return mButtonIds[direction];
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

  public void onPointerDown(int id, float x, float y)
  {
    mPointerId = id;
    setDpadState((int)x, (int)y);
  }

  public void onPointerMove(int id, float x, float y)
  {
    setDpadState((int)x, (int)y);
  }

  public void onPointerUp(int id, float x, float y)
  {
    mPointerId = -1;
    setDpadState((int)x, (int)y);
  }

  public void setPosition(int x, int y)
  {
    mControlPositionX = x;
    mControlPositionY = y;
  }

  public Rect getBounds()
  {
    return mDefaultStateBitmap.getBounds();
  }

  public void setBounds(Rect bounds)
  {
    mDefaultStateBitmap.setBounds(bounds);
    mPressedOneDirectionStateBitmap.setBounds(bounds);
    mPressedTwoDirectionsStateBitmap.setBounds(bounds);
  }

  public void setAlpha(int value)
  {
    mDefaultStateBitmap.setAlpha(value);
  }

  private void setDpadState(int pointerX, int pointerY)
  {
    // Up, Down, Left, Right
    boolean[] pressed = {false, false, false, false};

    if(mPointerId != -1)
    {
      Rect bounds = getBounds();

      if (bounds.top + (bounds.height() / 3) > pointerY)
        pressed[0] = true;
      else if (bounds.bottom - (bounds.height() / 3) < pointerY)
        pressed[1] = true;
      if (bounds.left + (bounds.width() / 3) > pointerX)
        pressed[2] = true;
      else if (bounds.right - (bounds.width() / 3) < pointerX)
        pressed[3] = true;
    }

    for(int i = 0; i < pressed.length; ++i)
    {
      if(pressed[i] != mPressStates[i])
      {
        NativeLibrary
          .onGamePadEvent(NativeLibrary.TouchScreenDevice, mButtonIds[i],
            pressed[i] ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
      }
    }

    mPressStates[0] = pressed[0];
    mPressStates[1] = pressed[1];
    mPressStates[2] = pressed[2];
    mPressStates[3] = pressed[3];
  }
}
