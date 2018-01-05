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
import android.view.View;

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableJoystick
{
	private final int[] axisIDs = {0, 0, 0, 0};
	private final float[] axises = {0f, 0f};
	private int trackId = -1;
	private int mJoystickType;
	private int mControlPositionX, mControlPositionY;
	private int mPreviousTouchX, mPreviousTouchY;
	private int mWidth;
	private int mHeight;
	private BitmapDrawable mOuterBitmap;
	private BitmapDrawable mDefaultStateInnerBitmap;
	private BitmapDrawable mPressedStateInnerBitmap;
	private boolean mPressedState = false;

	/**
	 * Constructor
	 *
	 * @param res                {@link Resources} instance.
	 * @param bitmapOuter        {@link Bitmap} which represents the outer non-movable part of the joystick.
	 * @param bitmapInnerDefault {@link Bitmap} which represents the default inner movable part of the joystick.
	 * @param bitmapInnerPressed {@link Bitmap} which represents the pressed inner movable part of the joystick.
	 * @param rectOuter          {@link Rect} which represents the outer joystick bounds.
	 * @param rectInner          {@link Rect} which represents the inner joystick bounds.
	 * @param joystick           Identifier for which joystick this is.
	 */
	public InputOverlayDrawableJoystick(Resources res, Bitmap bitmapOuter,
					    Bitmap bitmapInnerDefault, Bitmap bitmapInnerPressed,
					    Rect rectOuter, Rect rectInner, int joystick)
	{
		axisIDs[0] = joystick + 1;
		axisIDs[1] = joystick + 2;
		axisIDs[2] = joystick + 3;
		axisIDs[3] = joystick + 4;
		mJoystickType = joystick;

		mOuterBitmap = new BitmapDrawable(res, bitmapOuter);
		mDefaultStateInnerBitmap = new BitmapDrawable(res, bitmapInnerDefault);
		mPressedStateInnerBitmap = new BitmapDrawable(res, bitmapInnerPressed);
		mWidth = bitmapOuter.getWidth();
		mHeight = bitmapOuter.getHeight();

		setBounds(rectOuter);
		mDefaultStateInnerBitmap.setBounds(rectInner);
		mPressedStateInnerBitmap.setBounds(rectInner);
		SetInnerBounds();
	}

	/**
	 * Gets this InputOverlayDrawableJoystick's button ID.
	 *
	 * @return this InputOverlayDrawableJoystick's button ID.
	 */
	public int getId()
	{
		return mJoystickType;
	}

	public void draw(Canvas canvas)
	{
		mOuterBitmap.draw(canvas);
		getCurrentStateBitmapDrawable().draw(canvas);
	}

	public void TrackEvent(MotionEvent event)
	{
		int pointerIndex = event.getActionIndex();

		switch(event.getAction() & MotionEvent.ACTION_MASK)
		{
		case MotionEvent.ACTION_DOWN:
		case MotionEvent.ACTION_POINTER_DOWN:
			if (getBounds().contains((int)event.getX(pointerIndex), (int)event.getY(pointerIndex)))
			{
				mPressedState = true;
				trackId = event.getPointerId(pointerIndex);
			}
			break;
		case MotionEvent.ACTION_UP:
		case MotionEvent.ACTION_POINTER_UP:
			if (trackId == event.getPointerId(pointerIndex))
			{
				mPressedState = false;
				axises[0] = axises[1] = 0.0f;
				SetInnerBounds();
				trackId = -1;
			}
			break;
		}

		if (trackId == -1)
			return;

		for (int i = 0; i < event.getPointerCount(); i++)
		{
			if (trackId == event.getPointerId(i))
			{
				float touchX = event.getX(i);
				float touchY = event.getY(i);
				float maxY = getBounds().bottom;
				float maxX = getBounds().right;
				touchX -= getBounds().centerX();
				maxX -= getBounds().centerX();
				touchY -= getBounds().centerY();
				maxY -= getBounds().centerY();
				final float AxisX = touchX / maxX;
				final float AxisY = touchY / maxY;
				axises[0] = AxisY;
				axises[1] = AxisX;

				SetInnerBounds();
			}
		}
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
				int deltaX = fingerPositionX - mPreviousTouchX;
				int deltaY = fingerPositionY - mPreviousTouchY;
				mControlPositionX += deltaX;
				mControlPositionY += deltaY;
				setBounds(new Rect(mControlPositionX, mControlPositionY,
						   mOuterBitmap.getIntrinsicWidth() + mControlPositionX,
						   mOuterBitmap.getIntrinsicHeight() + mControlPositionY));
				SetInnerBounds();
				mPreviousTouchX = fingerPositionX;
				mPreviousTouchY = fingerPositionY;
				break;
		}
		return true;
	}


	public float[] getAxisValues()
	{
		float[] joyaxises = {0f, 0f, 0f, 0f};
		joyaxises[1] = Math.min(axises[0], 1.0f);
		joyaxises[0] = Math.min(axises[0], 0.0f);
		joyaxises[3] = Math.min(axises[1], 1.0f);
		joyaxises[2] = Math.min(axises[1], 0.0f);
		return joyaxises;
	}

	public int[] getAxisIDs()
	{
		return axisIDs;
	}

	private void SetInnerBounds()
	{
		int X = getBounds().centerX() + (int)((axises[1]) * (getBounds().width() / 2));
		int Y = getBounds().centerY() + (int)((axises[0]) * (getBounds().height() / 2));

		if (X > getBounds().centerX() + (getBounds().width() / 2))
			X = getBounds().centerX() + (getBounds().width() / 2);
		if (X < getBounds().centerX() - (getBounds().width() / 2))
			X = getBounds().centerX() - (getBounds().width() / 2);
		if (Y > getBounds().centerY() + (getBounds().height() / 2))
			Y = getBounds().centerY() + (getBounds().height() / 2);
		if (Y < getBounds().centerY() - (getBounds().height() / 2))
			Y = getBounds().centerY() - (getBounds().height() / 2);

		int width = mPressedStateInnerBitmap.getBounds().width() / 2;
		int height = mPressedStateInnerBitmap.getBounds().height() / 2;
		mDefaultStateInnerBitmap.setBounds(X - width, Y - height, X + width,  Y + height);
		mPressedStateInnerBitmap.setBounds(mDefaultStateInnerBitmap.getBounds());
	}

	public void setPosition(int x, int y)
	{
		mControlPositionX = x;
		mControlPositionY = y;
	}

	private BitmapDrawable getCurrentStateBitmapDrawable()
	{
		return mPressedState ? mPressedStateInnerBitmap : mDefaultStateInnerBitmap;
	}

	public void setBounds(Rect bounds)
	{
		mOuterBitmap.setBounds(bounds);
	}

	public Rect getBounds()
	{
		return mOuterBitmap.getBounds();
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
