package org.dolphinemu.dolphinemu.overlay;
import org.dolphinemu.dolphinemu.NativeLibrary;

public class InputOverlayPointer
{
  public final static int TYPE_OFF = 0;
  public final static int TYPE_CLICK = 1;
  public final static int TYPE_STICK = 2;

  private int mType;
  private int mPointerId;

  private float mDisplayScale;
  private float mScaledDensity;
  private int mMaxWidth;
  private int mMaxHeight;
  private float mGameWidthHalf;
  private float mGameHeightHalf;
  private float mAdjustX;
  private float mAdjustY;

  private int[] mAxisIDs = new int[4];

  // used for stick
  private float[] mAxises = new float[4];
  private float mCenterX;
  private float mCenterY;

  // used for click
  private long mLastClickTime;
  private int mClickButtonId;
  private boolean mIsDoubleClick;

  public InputOverlayPointer(int width, int height, float scaledDensity)
  {
    mType = TYPE_OFF;
    mAxisIDs[0] = 0;
    mAxisIDs[1] = 0;
    mAxisIDs[2] = 0;
    mAxisIDs[3] = 0;

    mDisplayScale = 1.0f;
    mScaledDensity = scaledDensity;
    mMaxWidth = width;
    mMaxHeight = height;
    mGameWidthHalf = width / 2.0f;
    mGameHeightHalf = height / 2.0f;
    mAdjustX = 1.0f;
    mAdjustY = 1.0f;

    mPointerId = -1;

    mLastClickTime = 0;
    mIsDoubleClick = false;
    mClickButtonId = NativeLibrary.ButtonType.WIIMOTE_BUTTON_A;

    if(NativeLibrary.IsRunning())
    {
      updateTouchPointer();
    }
  }

  public void updateTouchPointer()
  {
    float deviceAR = (float)mMaxWidth / (float)mMaxHeight;
    float gameAR = NativeLibrary.GetGameAspectRatio();
    // same scale ratio in renderbase.cpp
    mDisplayScale = (NativeLibrary.GetGameDisplayScale() - 1.0f) / 2.0f + 1.0f;

    if(gameAR <= deviceAR)
    {
      mAdjustX = gameAR / deviceAR;
      mAdjustY = 1.0f;
      mGameWidthHalf = Math.round(mMaxHeight * gameAR) / 2.0f;
      mGameHeightHalf = mMaxHeight / 2.0f;
    }
    else
    {
      mAdjustX = 1.0f;
      mAdjustY = gameAR / deviceAR;
      mGameWidthHalf = mMaxWidth / 2.0f;
      mGameHeightHalf = Math.round(mMaxWidth / gameAR) / 2.0f;
    }
  }

  public void setType(int type)
  {
    reset();
    mType = type;

    if(type == TYPE_CLICK)
    {
      // click
      mAxisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_IR + 1;
      mAxisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_IR + 2;
      mAxisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_IR + 3;
      mAxisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_IR + 4;
    }
    else if(type == TYPE_STICK)
    {
      // stick
      mAxisIDs[0] = NativeLibrary.ButtonType.WIIMOTE_IR + 1;
      mAxisIDs[1] = NativeLibrary.ButtonType.WIIMOTE_IR + 2;
      mAxisIDs[2] = NativeLibrary.ButtonType.WIIMOTE_IR + 3;
      mAxisIDs[3] = NativeLibrary.ButtonType.WIIMOTE_IR + 4;
    }
  }

  public void reset()
  {
    mPointerId = -1;
    for (int i = 0; i < 4; i++)
    {
      mAxises[i] = 0.0f;
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
        NativeLibrary.ButtonType.WIIMOTE_IR + i + 1, 0.0f);
    }
  }

  public int getPointerId()
  {
    return mPointerId;
  }

  public void onPointerDown(int id, float x, float y)
  {
    mPointerId = id;
    mCenterX = x;
    mCenterY = y;
    setPointerState(x, y);

    if(mType == TYPE_CLICK)
    {
      long currentTime = System.currentTimeMillis();
      if(currentTime - mLastClickTime < 300)
      {
        mIsDoubleClick = true;
        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, mClickButtonId, NativeLibrary.ButtonState.PRESSED);
      }
      mLastClickTime = currentTime;
    }
  }

  public void onPointerMove(int id, float x, float y)
  {
    setPointerState(x, y);
  }

  public void onPointerUp(int id, float x, float y)
  {
    mPointerId = -1;
    setPointerState(x, y);

    if(mIsDoubleClick)
    {
      NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, mClickButtonId, NativeLibrary.ButtonState.RELEASED);
      mIsDoubleClick = false;
    }
  }

  private void setPointerState(float x, float y)
  {
    float[] axises = new float[4];
    float scale = mDisplayScale;

    if(mType == TYPE_CLICK)
    {
      // click
      axises[0] = axises[1] = ((y * mAdjustY) - mGameHeightHalf) / mGameHeightHalf / scale;
      axises[2] = axises[3] = ((x * mAdjustX) - mGameWidthHalf) / mGameWidthHalf / scale;
    }
    else if(mType == TYPE_STICK)
    {
      // stick
      axises[0] = axises[1] = (y - mCenterY) / mGameHeightHalf * mScaledDensity / scale / 2.0f;
      axises[2] = axises[3] = (x - mCenterX) / mGameWidthHalf * mScaledDensity / scale / 2.0f;
    }

    for (int i = 0; i < mAxisIDs.length; ++i)
    {
      float value = mAxises[i] + axises[i];
      if (mPointerId == -1)
      {
        if(InputOverlay.sIRRecenter)
        {
          // recenter
          value = 0;
        }
        if(mType == TYPE_STICK)
        {
          // stick, save current value
          mAxises[i] = value;
        }
      }
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, mAxisIDs[i], value);
    }
  }
}
