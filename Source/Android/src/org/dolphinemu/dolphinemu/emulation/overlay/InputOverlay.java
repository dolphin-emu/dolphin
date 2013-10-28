/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.emulation.overlay;

import java.util.HashSet;
import java.util.Set;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonState;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonType;
import org.dolphinemu.dolphinemu.R;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
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
	private final Set<InputOverlayDrawable> overlayItems = new HashSet<InputOverlayDrawable>();

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
		overlayItems.add(initializeOverlayDrawable(context, R.drawable.button_a,     ButtonType.BUTTON_A));
		overlayItems.add(initializeOverlayDrawable(context, R.drawable.button_b,     ButtonType.BUTTON_B));
		overlayItems.add(initializeOverlayDrawable(context, R.drawable.button_start, ButtonType.BUTTON_START));

		// Set the on touch listener.
		setOnTouchListener(this);

		// Force draw
		setWillNotDraw(false);

		// Request focus for the overlay so it has priority on presses.
		requestFocus();
	}

	@Override
	public boolean onTouch(View v, MotionEvent event)
	{
		// Determine the button state to apply based on the MotionEvent action flag.
		int buttonState = (event.getAction() == MotionEvent.ACTION_DOWN) ? ButtonState.PRESSED : ButtonState.RELEASED;

		for (InputOverlayDrawable item : overlayItems)
		{
			// Check if there was a touch within the bounds of a drawable.
			if (item.getBounds().contains((int)event.getX(), (int)event.getY()))
			{
				switch (item.getId())
				{
					case ButtonType.BUTTON_A:
						NativeLibrary.onTouchEvent(ButtonType.BUTTON_A, buttonState);
						break;

					case ButtonType.BUTTON_B:
						NativeLibrary.onTouchEvent(ButtonType.BUTTON_B, buttonState);
						break;

					case ButtonType.BUTTON_START:
						NativeLibrary.onTouchEvent(ButtonType.BUTTON_START, buttonState);
						break;

					default:
						break;
				}
			}
		}

		return true;
	}

	@Override
	public void onDraw(Canvas canvas)
	{
		super.onDraw(canvas);

		// Draw all overlay items.
		for (InputOverlayDrawable item : overlayItems)
		{
			item.draw(canvas);
		}
	}

	/**
	 * Initializes an InputOverlayDrawable, given by resId, with all of the
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
	 * InputOverlayDrawable. Simply add it to the HashSet of overlay items and wait
	 * for Android to call the onDraw method.
	 * 
	 * @param context  The current {@link Context}.
	 * @param resId    The resource ID of the {@link Drawable} to get the {@link Bitmap} of.
	 * @param buttonId Identifier for determining what type of button the initialized InputOverlayDrawable represents.
	 * 
	 * @return An {@link InputOverlayDrawable} with the correct drawing bounds set.
	 *
	 */
	private static InputOverlayDrawable initializeOverlayDrawable(Context context, int resId, int buttonId)
	{
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawable.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Initialize the InputOverlayDrawable.
		final Bitmap bitmap = BitmapFactory.decodeResource(res, resId);
		final InputOverlayDrawable overlayDrawable = new InputOverlayDrawable(res, bitmap, buttonId);

		// String ID of the Drawable. This is what is passed into SharedPreferences
		// to check whether or not a value has been set.
		final String drawableId = res.getResourceEntryName(resId);

		// The X and Y coordinates of the InputOverlayDrawable on the InputOverlay.
		// These were set in the input overlay configuration menu.
		int drawableX = (int) sPrefs.getFloat(drawableId+"-X", 0f);
		int drawableY = (int) sPrefs.getFloat(drawableId+"-Y", 0f);

		// Intrinsic width and height of the InputOverlayDrawable.
		// For any who may not know, intrinsic width/height
		// are the original unmodified width and height of the image.
		int intrinWidth = overlayDrawable.getIntrinsicWidth();
		int intrinHeight = overlayDrawable.getIntrinsicHeight();

		// Now set the bounds for the InputOverlayDrawable.
		// This will dictate where on the screen (and the what the size) the InputOverlayDrawable will be.
		overlayDrawable.setBounds(drawableX, drawableY, drawableX+intrinWidth, drawableY+intrinHeight);

		return overlayDrawable;
	}
	
}
