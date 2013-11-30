/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.emulation.overlay;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnTouchListener;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonState;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonType;
import org.dolphinemu.dolphinemu.R;
import java.util.HashSet;
import java.util.Set;

/**
 * Draws the interactive input overlay on top of the
 * {@link NativeGLSurfaceView} that is rendering emulation.
 */
public final class InputOverlay extends SurfaceView implements OnTouchListener
{
	private final Set<InputOverlayDrawableButton> overlayButtons = new HashSet<InputOverlayDrawableButton>();
	private final Set<InputOverlayDrawableJoystick> overlayJoysticks = new HashSet<InputOverlayDrawableJoystick>();

	/**
	 * Resizes a {@link Bitmap} by a given scale factor
	 * 
	 * @param context The current {@link Context}
	 * @param bitmap  The {@link Bitmap} to scale.
	 * @param scale   The scale factor for the bitmap.
	 * 
	 * @return The scaled {@link Bitmap}
	 */
	public static Bitmap resizeBitmap(Context context, Bitmap bitmap, float scale)
	{
		// Retrieve screen dimensions.
		DisplayMetrics dm = context.getResources().getDisplayMetrics();

		Bitmap bitmapResized = Bitmap.createScaledBitmap(bitmap,
				(int)(dm.heightPixels * scale),
				(int)(dm.heightPixels * scale),
				false);
		return bitmapResized;
	}

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
		overlayButtons.add(initializeOverlayButton(context, R.drawable.gcpad_a, ButtonType.BUTTON_A));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.gcpad_b, ButtonType.BUTTON_B));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.gcpad_x, ButtonType.BUTTON_X));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.gcpad_y, ButtonType.BUTTON_Y));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.gcpad_z, ButtonType.BUTTON_Z));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.gcpad_start, ButtonType.BUTTON_START));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.gcpad_l, ButtonType.TRIGGER_L));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.gcpad_r, ButtonType.TRIGGER_R));
		overlayJoysticks.add(initializeOverlayJoystick(context,
				R.drawable.gcpad_joystick_range, R.drawable.gcpad_joystick,
				ButtonType.STICK_MAIN));

		// Set the on touch listener.
		setOnTouchListener(this);

		// Force draw
		setWillNotDraw(false);

		// Request focus for the overlay so it has priority on presses.
		requestFocus();
	}

	@Override
	public void draw(Canvas canvas)
	{
		super.onDraw(canvas);

		for (InputOverlayDrawableButton button : overlayButtons)
		{
			button.draw(canvas);
		}

		for (InputOverlayDrawableJoystick joystick: overlayJoysticks)
		{
			joystick.draw(canvas);
		}
	}

	@Override
	public boolean onTouch(View v, MotionEvent event)
	{
		// Determine the button state to apply based on the MotionEvent action flag.
		// TODO: This will not work when Axis support is added. Make sure to refactor this when that time comes
		// The reason it won't work is that when moving an axis, you would get the event MotionEvent.ACTION_MOVE.
		//
		// TODO: Refactor this so we detect either Axis movements or button presses so we don't run two loops all the time.
		//
		int buttonState = (event.getAction() == MotionEvent.ACTION_DOWN) ? ButtonState.PRESSED : ButtonState.RELEASED;
		// Check if there was a touch within the bounds of a drawable.
		for (InputOverlayDrawableButton button : overlayButtons)
		{
			if (button.getBounds().contains((int)event.getX(), (int)event.getY()))
				NativeLibrary.onTouchEvent(0, button.getId(), buttonState);
		}


		for (InputOverlayDrawableJoystick joystick : overlayJoysticks)
		{
			joystick.TrackEvent(event);
			int[] axisIDs = joystick.getAxisIDs();
			float[] axises = joystick.getAxisValues();

			for (int i = 0; i < 4; i++)
				NativeLibrary.onTouchAxisEvent(0, axisIDs[i], axises[i]);
		}

		return true;
	}

	/**
	 * Initializes an InputOverlayDrawableButton, given by resId, with all of the
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
	 * InputOverlayDrawableButton. Simply add it to the HashSet of overlay items and wait
	 * for Android to call the onDraw method.
	 * 
	 * @param context  The current {@link Context}.
	 * @param resId    The resource ID of the {@link Drawable} to get the {@link Bitmap} of.
	 * @param buttonId Identifier for determining what type of button the initialized InputOverlayDrawableButton represents.
	 * 
	 * @return An {@link InputOverlayDrawableButton} with the correct drawing bounds set.
	 *
	 */
	private static InputOverlayDrawableButton initializeOverlayButton(Context context, int resId, int buttonId)
	{
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Initialize the InputOverlayDrawableButton.
		final Bitmap bitmap = resizeBitmap(context, BitmapFactory.decodeResource(res, resId), 0.20f);
		final InputOverlayDrawableButton overlayDrawable = new InputOverlayDrawableButton(res, bitmap, buttonId);

		// String ID of the Drawable. This is what is passed into SharedPreferences
		// to check whether or not a value has been set.
		final String drawableId = res.getResourceEntryName(resId);

		// The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
		// These were set in the input overlay configuration menu.
		int drawableX = (int) sPrefs.getFloat(drawableId+"-X", 0f);
		int drawableY = (int) sPrefs.getFloat(drawableId+"-Y", 0f);

		// Intrinsic width and height of the InputOverlayDrawableButton.
		// For any who may not know, intrinsic width/height
		// are the original unmodified width and height of the image.
		int intrinWidth = overlayDrawable.getIntrinsicWidth();
		int intrinHeight = overlayDrawable.getIntrinsicHeight();

		// Now set the bounds for the InputOverlayDrawableButton.
		// This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
		overlayDrawable.setBounds(drawableX, drawableY, drawableX+intrinWidth, drawableY+intrinHeight);

		return overlayDrawable;
	}

	/**
	 * Initializes an {@link InputOverlayDrawableJoystick} 
	 * 
	 * @param context   The current {@link Context}
	 * @param resOuter  Resource ID for the outer image of the joystick (the static image that shows the circular bounds).
	 * @param resInner  Resource ID for the inner image of the joystick (the one you actually move around).
	 * @param joystick    Identifier for which joystick this is.
	 * 
	 * @return the initialized {@link InputOverlayDrawableJoystick}.
	 */
	private static InputOverlayDrawableJoystick initializeOverlayJoystick(Context context, int resOuter, int resInner, int joystick)
	{
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableJoystick.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Initialize the InputOverlayDrawableJoystick.
		final Bitmap bitmapOuter = resizeBitmap(context, BitmapFactory.decodeResource(res, resOuter), 0.30f);
		final Bitmap bitmapInner = BitmapFactory.decodeResource(res, resInner);

		// String ID of the Drawable. This is what is passed into SharedPreferences
		// to check whether or not a value has been set.
		final String drawableId = res.getResourceEntryName(resOuter);

		// The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
		// These were set in the input overlay configuration menu.
		int drawableX = (int) sPrefs.getFloat(drawableId+"-X", 0f);
		int drawableY = (int) sPrefs.getFloat(drawableId+"-Y", 0f);

		// Now set the bounds for the InputOverlayDrawableJoystick.
		// This will dictate where on the screen (and the what the size) the InputOverlayDrawableJoystick will be.
		int outerSize = bitmapOuter.getWidth();
		Rect outerRect = new Rect(drawableX, drawableY, drawableX + outerSize, drawableY + outerSize);
		Rect innerRect = new Rect(0, 0, outerSize / 4, outerSize / 4);

		final InputOverlayDrawableJoystick overlayDrawable
				= new InputOverlayDrawableJoystick(res,
					bitmapOuter, bitmapInner,
					outerRect, innerRect,
					joystick);


		return overlayDrawable;
	}
	
}
