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
import android.view.WindowManager;
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
	private Drawable resizeDrawable(WindowManager wm, Context context, Drawable image, float scale) {
		// Retrieve screen dimensions.
		DisplayMetrics displayMetrics = new DisplayMetrics();
		wm.getDefaultDisplay().getMetrics(displayMetrics);

		Bitmap b = ((BitmapDrawable)image).getBitmap();
		Bitmap bitmapResized = Bitmap.createScaledBitmap(b,
				(int)(displayMetrics.heightPixels * scale),
				(int)(displayMetrics.heightPixels * scale),
				false);
		return new BitmapDrawable(context.getResources(), bitmapResized);
	}

	/**
	 * Constructor
	 * 
	 * @param context    the current {@link Context}.
	 * @param buttonId   the String ID for this button.
	 * @param drawableId the Drawable ID for the image to represent this OverlayConfigButton.
	 */
	public OverlayConfigButton(WindowManager wm, Context context, String buttonId, int drawableId)
	{
		super(context);

		// Set the button ID.
		this.buttonId = buttonId;

		// Set the button as its own OnTouchListener.
		setOnTouchListener(this);

		// Set the button's icon that represents it.
		setBackground(resizeDrawable(wm, context,
						context.getResources().getDrawable(drawableId),
						drawableId == R.drawable.gcpad_joystick_range ? 0.30f : 0.20f));

		// Get the SharedPreferences instance.
		sharedPrefs = PreferenceManager.getDefaultSharedPreferences(context);

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
			// Only change the X/Y coordinates when we move the button.
			case MotionEvent.ACTION_MOVE:
			{
				setX(getX() + event.getX());
				setY(getY() + event.getY());
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
				editor.commit();
				return true;
			}
		}

		return false;
	}
}
