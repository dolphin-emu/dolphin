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

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableDpad
{
	// The ID identifying what type of button this Drawable represents.
	private int[] mButtonType = new int[4];
	private int mTrackId;
	private int mPreviousTouchX, mPreviousTouchY;
	private int mControlPositionX, mControlPositionY;
	private int mWidth;
	private int mHeight;
	private BitmapDrawable mDefaultStateBitmap;
	private BitmapDrawable mPressedOneDirectionStateBitmap;
	private BitmapDrawable mPressedTwoDirectionsStateBitmap;
	private int mPressState = STATE_DEFAULT;

	public static final int STATE_DEFAULT = 0;
	public static final int STATE_PRESSED_UP = 1;
	public static final int STATE_PRESSED_DOWN = 2;
	public static final int STATE_PRESSED_LEFT = 3;
	public static final int STATE_PRESSED_RIGHT = 4;
	public static final int STATE_PRESSED_UP_LEFT = 5;
	public static final int STATE_PRESSED_UP_RIGHT = 6;
	public static final int STATE_PRESSED_DOWN_LEFT = 7;
	public static final int STATE_PRESSED_DOWN_RIGHT = 8;

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
	}

	public void draw(Canvas canvas)
	{
		int px = mControlPositionX + (getWidth()/2);
		int py = mControlPositionY + (getHeight()/2);
		switch (mPressState) {
			case STATE_DEFAULT:
				mDefaultStateBitmap.draw(canvas);
				break;
			case STATE_PRESSED_UP:
				mPressedOneDirectionStateBitmap.draw(canvas);
				break;
			case STATE_PRESSED_RIGHT:
				canvas.save();
				canvas.rotate(90, px, py);
				mPressedOneDirectionStateBitmap.draw(canvas);
				canvas.restore();
				break;
			case STATE_PRESSED_DOWN:
				canvas.save();
				canvas.rotate(180, px, py);
				mPressedOneDirectionStateBitmap.draw(canvas);
				canvas.restore();
				break;
			case STATE_PRESSED_LEFT:
				canvas.save();
				canvas.rotate(270, px, py);
				mPressedOneDirectionStateBitmap.draw(canvas);
				canvas.restore();
				break;
			case STATE_PRESSED_UP_LEFT:
				mPressedTwoDirectionsStateBitmap.draw(canvas);
				break;
			case STATE_PRESSED_UP_RIGHT:
				canvas.save();
				canvas.rotate(90, px, py);
				mPressedTwoDirectionsStateBitmap.draw(canvas);
				canvas.restore();
				break;
			case STATE_PRESSED_DOWN_RIGHT:
				canvas.save();
				canvas.rotate(180, px, py);
				mPressedTwoDirectionsStateBitmap.draw(canvas);
				canvas.restore();
				break;
			case STATE_PRESSED_DOWN_LEFT:
				canvas.save();
				canvas.rotate(270, px, py);
				mPressedTwoDirectionsStateBitmap.draw(canvas);
				canvas.restore();
				break;
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

	public void setTrackId(int trackId)
	{
		mTrackId = trackId;
	}

	public int getTrackId()
	{
		return mTrackId;
	}

	public boolean onConfigureTouch(MotionEvent event)
	{
		int pointerIndex = event.getActionIndex();
		int fingerPositionX = (int)event.getX(pointerIndex);
		int fingerPositionY = (int)event.getY(pointerIndex);
		switch (event.getAction())
		{
			case MotionEvent.ACTION_DOWN:
				mPreviousTouchX = fingerPositionX;
				mPreviousTouchY = fingerPositionY;
				break;
			case MotionEvent.ACTION_MOVE:
				mControlPositionX += fingerPositionX - mPreviousTouchX;
				mControlPositionY += fingerPositionY - mPreviousTouchY;
				setBounds(mControlPositionX, mControlPositionY, getWidth() + mControlPositionX, getHeight() + mControlPositionY);
				mPreviousTouchX = fingerPositionX;
				mPreviousTouchY = fingerPositionY;
				break;

		}
		return true;
	}

	public void setPosition(int x, int y)
	{
		mControlPositionX = x;
		mControlPositionY = y;
	}

	public void setBounds(int left, int top, int right, int bottom)
	{
		mDefaultStateBitmap.setBounds(left, top, right, bottom);
		mPressedOneDirectionStateBitmap.setBounds(left, top, right, bottom);
		mPressedTwoDirectionsStateBitmap.setBounds(left, top, right, bottom);
	}

	public Rect getBounds()
	{
		return mDefaultStateBitmap.getBounds();
	}

	public int getWidth() {
		return mWidth;
	}

	public int getHeight() {
		return mHeight;
	}

	public void setState(int pressState) {
		mPressState = pressState;
	}
}
