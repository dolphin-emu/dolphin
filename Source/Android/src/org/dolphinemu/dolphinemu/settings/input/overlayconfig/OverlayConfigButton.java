/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.input.overlayconfig;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.preference.PreferenceManager;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.widget.Button;
import org.dolphinemu.dolphinemu.R;

/**
 * A movable {@link Button} for use within the
 * input overlay configuration screen.
 */
public final class OverlayConfigButton extends Button implements OnTouchListener
{
	// SharedPreferences instance that the button positions are cached to.
	private final SharedPreferences sharedPrefs;

	// The String ID for this button.
	//
	// This ID is used upon releasing this button as the key for saving 
	// the X and Y coordinates of this button. This same key is also used
	// for setting the coordinates of the button on the actual overlay during emulation.
	//
	// They can be accessed through SharedPreferences respectively as follows:
	//
	// SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(this);
	// float buttonX = sPrefs.getFloat(buttonId+"-X", -1f);
	// float buttonY = sPrefs.getFloat(buttonId+"-Y", -1f);
	//
	private final String buttonId;

	// The offset of the press while moving the button
	private float moveOffsetX, moveOffsetY;

	private Drawable resizeDrawable(Drawable image, float scale)
	{
		// Retrieve screen dimensions.
		DisplayMetrics displayMetrics = getResources().getDisplayMetrics();

		Bitmap b = ((BitmapDrawable)image).getBitmap();
		Bitmap bitmapResized = Bitmap.createScaledBitmap(b,
				(int)(displayMetrics.heightPixels * scale),
				(int)(displayMetrics.heightPixels * scale),
				true);

		return new BitmapDrawable(getResources(), bitmapResized);
	}

	/**
	 * Constructor
	 * 
	 * @param context    the current {@link Context}.
	 * @param buttonId   the String ID for this button.
	 * @param drawableId the Drawable ID for the image to represent this OverlayConfigButton.
	 */
	public OverlayConfigButton(Context context, String buttonId, int drawableId)
	{
		super(context);

		// Set the button ID.
		this.buttonId = buttonId;

		// Set the button as its own OnTouchListener.
		setOnTouchListener(this);

		// Get the SharedPreferences instance.
		sharedPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Decide scale based on button ID

		// SeekBars are not able to set a minimum value, only a maximum value, which complicates
		// things a bit. What happens here is after the SeekBar's value is retrieved, 25 is
		// added so the value will never go below 25. It is then divided by 50 (25 + 25) so the
		// default value will be 100%.
		float scale;
		float overlaySize = sharedPrefs.getInt("controls_size", 25);
		overlaySize += 25;
		overlaySize /= 50;

		switch (drawableId)
		{
		case R.drawable.gcpad_b:
			scale = 0.13f * overlaySize;
			break;
		case R.drawable.gcpad_x:
		case R.drawable.gcpad_y:
			scale = 0.18f * overlaySize;
			break;
		case R.drawable.gcpad_start:
			scale = 0.12f * overlaySize;
			break;
		case R.drawable.gcpad_joystick_range:
			scale = 0.30f * overlaySize;
			break;
		default:
			scale = 0.20f * overlaySize;
			break;
		}

		// Set the button's icon that represents it.
		setBackground(resizeDrawable(getResources().getDrawable(drawableId), scale));

		// Check if this button has previous values set that aren't the default.
		final float x = sharedPrefs.getFloat(buttonId+"-X", -1f);
		final float y = sharedPrefs.getFloat(buttonId+"-Y", -1f);

		// If they are not -1, then they have a previous value set.
		// Thus, we set those coordinate values.
		if (x != -1f && y != -1f)
		{
			setX(x);
			setY(y);
		}
	}

	@Override
	public boolean onTouch(View v, MotionEvent event)
	{
		switch(event.getAction())
		{
			// Get the offset of the press within the button
			// The event X and Y locations are the offset within the button, not the screen
			case MotionEvent.ACTION_DOWN:
			{
				moveOffsetX = event.getX();
				moveOffsetY = event.getY();
				return true;
			}

			// Only change the X/Y coordinates when we move the button.
			case MotionEvent.ACTION_MOVE:
			{
				setX(getX() + event.getX() - moveOffsetX);
				setY(getY() + event.getY() - moveOffsetY);
				return true;
			}

			// Whenever the press event has ended
			// is when we save all of the information.
			case MotionEvent.ACTION_UP:
			{
				// Add the current X and Y positions of this button into SharedPreferences.
				SharedPreferences.Editor editor = sharedPrefs.edit();
				editor.putFloat(buttonId+"-X", getX());
				editor.putFloat(buttonId+"-Y", getY());
				editor.apply();
				return true;
			}
		}

		return false;
	}
}
