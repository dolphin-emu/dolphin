/**
 * Copyright 2016 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.view.MotionEvent;
import android.view.View;

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableDpad extends BitmapDrawable
{
	// The ID identifying what type of button this Drawable represents.
	private int[] mButtonType = new int[4];
	private int mPreviousTouchX, mPreviousTouchY;
	private int mControlPositionX, mControlPositionY;

	/**
	 * Constructor
	 *
	 * @param res         {@link Resources} instance.
	 * @param bitmap      {@link Bitmap} to use with this Drawable.
	 * @param buttonUp    Identifier for the up button.
	 * @param buttonDown  Identifier for the down button.
	 * @param buttonLeft  Identifier for the left button.
	 * @param buttonRight Identifier for the right button.
	 */
	public InputOverlayDrawableDpad(Resources res, Bitmap bitmap,
					int buttonUp, int buttonDown,
					int buttonLeft, int buttonRight)
	{
		super(res, bitmap);
		mButtonType[0] = buttonUp;
		mButtonType[1] = buttonDown;
		mButtonType[2] = buttonLeft;
		mButtonType[3] = buttonRight;
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
				setBounds(new Rect(mControlPositionX, mControlPositionY, getBitmap().getWidth() + mControlPositionX, getBitmap().getHeight() + mControlPositionY));
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
}
