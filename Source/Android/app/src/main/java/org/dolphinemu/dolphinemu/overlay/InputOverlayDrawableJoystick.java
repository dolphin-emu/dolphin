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

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableJoystick extends BitmapDrawable
{
	private final int[] axisIDs = {0, 0, 0, 0};
	private final float[] axises = {0f, 0f};
	private final BitmapDrawable ringInner;
	private int trackId = -1;

	/**
	 * Constructor
	 *
	 * @param res         {@link Resources} instance.
	 * @param bitmapOuter {@link Bitmap} which represents the outer non-movable part of the joystick.
	 * @param bitmapInner {@link Bitmap} which represents the inner movable part of the joystick.
	 * @param rectOuter   {@link Rect} which represents the outer joystick bounds.
	 * @param rectInner   {@link Rect} which represents the inner joystick bounds.
	 * @param joystick    Identifier for which joystick this is.
	 */
	public InputOverlayDrawableJoystick(Resources res,
	                                    Bitmap bitmapOuter, Bitmap bitmapInner,
	                                    Rect rectOuter, Rect rectInner,
	                                    int joystick)
	{
		super(res, bitmapOuter);
		this.setBounds(rectOuter);

		this.ringInner = new BitmapDrawable(res, bitmapInner);
		this.ringInner.setBounds(rectInner);
		SetInnerBounds();
		this.axisIDs[0] = joystick + 1;
		this.axisIDs[1] = joystick + 2;
		this.axisIDs[2] = joystick + 3;
		this.axisIDs[3] = joystick + 4;
	}

	@Override
	public void draw(Canvas canvas)
	{
		super.draw(canvas);

		ringInner.draw(canvas);
	}

	public void TrackEvent(MotionEvent event)
	{
		int pointerIndex = event.getActionIndex();

		switch(event.getAction() & MotionEvent.ACTION_MASK)
		{
		case MotionEvent.ACTION_DOWN:
		case MotionEvent.ACTION_POINTER_DOWN:
			if (getBounds().contains((int)event.getX(pointerIndex), (int)event.getY(pointerIndex)))
				trackId = event.getPointerId(pointerIndex);
			break;
		case MotionEvent.ACTION_UP:
		case MotionEvent.ACTION_POINTER_UP:
			if (trackId == event.getPointerId(pointerIndex))
			{
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
		float floatX = this.getBounds().centerX();
		float floatY = this.getBounds().centerY();
		floatY += axises[0] * (this.getBounds().height() / 2);
		floatX += axises[1] * (this.getBounds().width() / 2);
		int X = (int)(floatX);
		int Y = (int)(floatY);
		int width = this.ringInner.getBounds().width() / 2;
		int height = this.ringInner.getBounds().height() / 2;
		this.ringInner.setBounds(X - width, Y - height,
				X + width,  Y + height);
	}
}
