/**
 * Copyright 2013 Dolphin Emulator Project
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
public final class InputOverlayDrawableButton extends BitmapDrawable
{
	// The ID identifying what type of button this Drawable represents.
	private int mButtonType;
	private int mPreviousTouchX, mPreviousTouchY;
	private int mControlPositionX, mControlPositionY;
	private String mSharedPrefsId;

	/**
	 * Constructor
	 * 
	 * @param res         {@link Resources} instance.
	 * @param bitmap      {@link Bitmap} to use with this Drawable.
	 * @param buttonType  Identifier for this type of button.
	 * @param sharedPrefsId  Identifier for getting X and Y control positions from Shared Preferences.
	 */
	public InputOverlayDrawableButton(Resources res, Bitmap bitmap, int buttonType, String sharedPrefsId)
	{
		super(res, bitmap);
		mButtonType = buttonType;
		mSharedPrefsId = sharedPrefsId;
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

	public boolean onConfigureTouch(View v, MotionEvent event)
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

	public String getSharedPrefsId()
	{
		return mSharedPrefsId;
	}

	public void setPosition(int x, int y)
	{
		mControlPositionX = x;
		mControlPositionY = y;
	}
}
