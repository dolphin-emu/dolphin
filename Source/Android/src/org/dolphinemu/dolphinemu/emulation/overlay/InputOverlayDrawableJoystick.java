/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.emulation.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.view.MotionEvent;

/**
 * Custom {@link android.graphics.drawable.BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public class InputOverlayDrawableJoystick extends BitmapDrawable
{
	// The ID identifying what type of button this Drawable represents.
	private int axisIDs[] = {0, 0, 0, 0};
	private float axises[] = {0f, 0f};
	private int trackid = -1;
	private BitmapDrawable ringInner;

	/**
	 * Constructor
	 *
	 * @param res         {@link android.content.res.Resources} instance.
	 * @param bitmapOuter      {@link android.graphics.Bitmap} to use with this Drawable.
	 * @param axisUp      Identifier for this type of axis.
	 * @param axisDown    Identifier for this type of axis.
	 * @param axisLeft    Identifier for this type of axis.
	 * @param axisRight   Identifier for this type of axis.
	 */
	public InputOverlayDrawableJoystick(Resources res,
	                                    Bitmap bitmapOuter, Bitmap bitmapInner,
	                                    Rect rectOuter, Rect rectInner,
	                                    int axisUp, int axisDown, int axisLeft, int axisRight)
	{
		super(res, bitmapOuter);
		this.setBounds(rectOuter);

		this.ringInner = new BitmapDrawable(res, bitmapInner);
		this.ringInner.setBounds(rectInner);
		SetInnerBounds();
		this.axisIDs[0] = axisUp;
		this.axisIDs[1] = axisDown;
		this.axisIDs[2] = axisLeft;
		this.axisIDs[3] = axisRight;
	}
	public void Draw(Canvas canvas)
	{
		this.draw(canvas);
		ringInner.draw(canvas);
	}
	public void TrackEvent(MotionEvent event)
	{
		int pointerIndex = event.getActionIndex();
		if (trackid == -1)
		{
			if (event.getAction() == MotionEvent.ACTION_DOWN)
				if(getBounds().contains((int)event.getX(), (int)event.getY()))
					trackid = event.getPointerId(pointerIndex);
		}
		else
		{
			if (event.getAction() == MotionEvent.ACTION_UP)
			{
				if (trackid == event.getPointerId(pointerIndex))
				{
					axises[0] = axises[1] = 0.0f;
					SetInnerBounds();
					trackid = -1;
				}
			}
		}
		if (trackid == -1)
			return;
		float touchX = event.getX();
		float touchY = event.getY();
		float maxY = this.getBounds().bottom;
		float maxX = this.getBounds().right;
		touchX -= this.getBounds().centerX();
		maxX -= this.getBounds().centerX();
		touchY -= this.getBounds().centerY();
		maxY -= this.getBounds().centerY();
		final float AxisX = touchX / maxX;
		final float AxisY = touchY/maxY;

		this.axises[0] = AxisY;
		this.axises[1] = AxisX;

		SetInnerBounds();
	}
	public float[] getAxisValues()
	{
		float[] joyaxises = new float[4];
		if (axises[0] >= 0.0f)
		{
			joyaxises[0] = 0.0f;
			joyaxises[1] = Math.min(axises[0], 1.0f);
		}
		else
		{
			joyaxises[0] = Math.min(Math.abs(axises[0]), 1.0f);
			joyaxises[1] = 0.0f;
		}
		if (axises[1] >= 0.0)
		{
			joyaxises[2] = 0.0f;
			joyaxises[3] = Math.min(axises[1], 1.0f);
		}
		else
		{
			joyaxises[2] = Math.min(Math.abs(axises[1]), 1.0f);
			joyaxises[3] = 0.0f;
		}
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
