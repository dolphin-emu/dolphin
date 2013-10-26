/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.emulation.overlay;

import java.util.HashSet;
import java.util.Set;

import org.dolphinemu.dolphinemu.R;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnTouchListener;

/**
 * Draws the interactive input overlay on top of the
 * {@link NativeGLSurfaceView} that is rendering emulation.
 */
public final class InputOverlay extends SurfaceView implements OnTouchListener
{
	private final Set<BitmapDrawable> overlayItems = new HashSet<BitmapDrawable>();

	/**
	 * Constructor
	 * 
	 * @param context The current {@link Context}.
	 * @param attrs   {@link AttributeSet} for parsing XML attributes.
	 */
	public InputOverlay(Context context, AttributeSet attrs)
	{
		super(context, attrs);

		// Add all the overlay items to the HashSet.
		overlayItems.add(initializeOverlayDrawable(context, R.drawable.button_a));
		overlayItems.add(initializeOverlayDrawable(context, R.drawable.button_b));
		overlayItems.add(initializeOverlayDrawable(context, R.drawable.button_start));

		// Force draw
		setWillNotDraw(false);

		// Request focus for the overlay so it has priority on presses.
		requestFocus();
	}

	@Override
	public boolean onTouch(View v, MotionEvent event)
	{
		switch (event.getAction())
		{
			case MotionEvent.ACTION_DOWN:
			{
				// TODO: Handle down presses.
				return true;
			}
		}

		return false;
	}

	@Override
	public void onDraw(Canvas canvas)
	{
		super.onDraw(canvas);

		// Draw all overlay items.
		for (BitmapDrawable item : overlayItems)
		{
			item.draw(canvas);
		}
	}

	/**
	 * Initializes a drawable, given by resId, with all of the
	 * parameters set for it to be properly shown on the InputOverlay. 
	 * <p>
	 * This works due to the way the X and Y coordinates are stored within
	 * the {@link SharedPreferences}.
	 * <p>
	 * In the input overlay configuration menu,
	 * once a touch event begins and then ends (ie. Organizing the buttons to one's own liking for the overlay).
	 * the X and Y coordinates of the button at the END of its touch event
	 * (when you remove your finger/stylus from the touchscreen) are then stored
	 * within a SharedPreferences instance so that those values can be retrieved here.
	 * <p>
	 * This has a few benefits over the conventional way of storing the values
	 * (ie. within the Dolphin ini file).
	 * <ul>
	 *     <li>No native calls</li>
	 *     <li>Keeps Android-only values inside the Android environment</li>
	 * </ul>
	 * <p>
	 * Technically no modifications should need to be performed on the returned
	 * BitmapDrawable. Simply add it to the HashSet of overlay items and wait
	 * for Android to call the onDraw method.
	 * 
	 * @param context The current {@link Context}.
	 * @param resId   The resource ID of the {@link BitmapDrawable} to get.
	 * 
	 * @return A {@link BitmapDrawable} with the correct drawing bounds set.
	 *
	 */
	private static BitmapDrawable initializeOverlayDrawable(Context context, int resId)
	{
		// Resources handle for fetching the drawable, etc.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the drawable.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Get the desired drawable.
		BitmapDrawable drawable = (BitmapDrawable) res.getDrawable(resId);

		// String ID of the drawable. This is what is passed into SharedPreferences
		// to check whether or not a value has been set.
		String drawableId = res.getResourceEntryName(resId);

		// The X and Y coordinates of the drawable on the InputOverlay.
		// These were set in the input overlay configuration menu.
		int drawableX = (int) sPrefs.getFloat(drawableId+"-X", 0f);
		int drawableY = (int) sPrefs.getFloat(drawableId+"-Y", 0f);

		// Intrinsic width and height of the drawable.
		// For any who may not know, intrinsic width/height
		// are the original unmodified width and height of the image.
		int intrinWidth = drawable.getIntrinsicWidth();
		int intrinHeight = drawable.getIntrinsicHeight();

		// Now set the bounds for the drawable.
		// This will dictate where on the screen (and the what the size) of the drawable will be.
		drawable.setBounds(drawableX, drawableY, drawableX+intrinWidth, drawableY+intrinHeight);

		return drawable;
	}
	
}
