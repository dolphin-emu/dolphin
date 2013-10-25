/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.input;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.widget.Button;

/**
 * A movable {@link Button} for use within the
 * input overlay configuration screen.
 */
public final class InputOverlayConfigButton extends Button implements OnTouchListener
{
	/**
	 * Constructor
	 * 
	 * @param context The current {@link Context}.
	 * @param attribs {@link AttributeSet} for parsing XML attributes.
	 */
	public InputOverlayConfigButton(Context context, AttributeSet attribs)
	{
		super(context, attribs);

		// Set the button as its own OnTouchListener.
		setOnTouchListener(this);
	}

	@Override
	public boolean onTouch(View v, MotionEvent event)
	{
		switch(event.getAction())
		{
			// Only change the X/Y coordinates when we move the button.
			case MotionEvent.ACTION_MOVE:
			{
				setX(getX() + event.getX());
				setY(getY() + event.getY());
				return true;
			}
		}

		return false;
	}
}
