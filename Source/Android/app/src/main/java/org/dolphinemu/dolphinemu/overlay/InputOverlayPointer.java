package org.dolphinemu.dolphinemu.overlay;

import org.dolphinemu.dolphinemu.NativeLibrary;

public class InputOverlayPointer
{
  private int mTrackId;
  private float mWidth;
  private float mHeight;
  private float mCenterX;
  private float mCenterY;
  private float mPointerX;
  private float mPointerY;
  private final int[] mAxisIDs = new int[4];

  public InputOverlayPointer(float width, float height)
  {
    mAxisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_IR + 1;
    mAxisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_IR + 2;
    mAxisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_IR + 3;
    mAxisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_IR + 4;

    mPointerX = 0;
    mPointerY = 0;

    mCenterX = 0;
    mCenterY = 0;

    mWidth = width;
    mHeight = height;

    mTrackId = -1;
  }

  public int getTrackId()
  {
    return mTrackId;
  }

  public void onPointerDown(int id, float x, float y)
  {
    mTrackId = id;
    mCenterX = x;
    mCenterY = y;
    setPointerState(x, y);
  }

  public void onPointerMove(int id, float x, float y)
  {
    setPointerState(x, y);
  }

  public void onPointerUp(int id, float x, float y)
  {
    mTrackId = -1;
    mCenterX = 0;
    mCenterY = 0;
    setPointerState(0, 0);
  }

  private void setPointerState(float x, float y)
  {
    mPointerX = x;
    mPointerY = y;

    float axises[] = new float[4];
    axises[0] = axises[1] = (mCenterY - mPointerY) / mHeight;
    axises[2] = axises[3] = (mCenterX - mPointerX) / mWidth;

    for (int i = 0; i < 4; i++)
    {
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, mAxisIDs[i], axises[i]);
    }
  }
}
