/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.emulation.overlay;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnTouchListener;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonState;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonType;
import org.dolphinemu.dolphinemu.R;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;

/**
 * Draws the interactive input overlay on top of the
 * {@link NativeGLSurfaceView} that is rendering emulation.
 */
public final class InputOverlay extends SurfaceView implements OnTouchListener, Runnable
{
	private final Set<InputOverlayDrawableButton> overlayButtons = new HashSet<InputOverlayDrawableButton>();
	private final Set<InputOverlayDrawableJoystick> overlayJoysticks = new HashSet<InputOverlayDrawableJoystick>();
	Semaphore _sema;

	SurfaceHolder surfaceHolder;
	Thread thread = null;
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
		overlayButtons.add(initializeOverlayButton(context, R.drawable.button_a, ButtonType.BUTTON_A));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.button_b, ButtonType.BUTTON_B));
		overlayButtons.add(initializeOverlayButton(context, R.drawable.button_start, ButtonType.BUTTON_START));
		overlayJoysticks.add(initializeOverlayJoystick(context,
				R.drawable.joy_outer, R.drawable.joy_inner,
				ButtonType.STICK_MAIN_UP, ButtonType.STICK_MAIN_DOWN,
				ButtonType.STICK_MAIN_LEFT, ButtonType.STICK_MAIN_RIGHT));

		// Set the on touch listener.
		setOnTouchListener(this);

		// Force draw
		setWillNotDraw(false);

		// Request focus for the overlay so it has priority on presses.
		requestFocus();
		_sema = new Semaphore(0);

		surfaceHolder = getHolder();
		surfaceHolder.setFormat(PixelFormat.TRANSPARENT);
		thread = new Thread(this);
		thread.start();
	}
	private boolean Draw()
	{
		if(surfaceHolder.getSurface().isValid()){
			// Draw everything
			Canvas canvas = surfaceHolder.lockCanvas();
			canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);
			for (InputOverlayDrawableButton item : overlayButtons)
				item.draw(canvas);
			for (InputOverlayDrawableJoystick item : overlayJoysticks)
				item.Draw(canvas);
			surfaceHolder.unlockCanvasAndPost(canvas);
			return true;
		}
		return false;
	}
	@Override
	public void run() {
		boolean didFirstPost = false;
		while(!didFirstPost)
			if (Draw())
				didFirstPost = true;
		while(true){
			try
			{
				_sema.acquire();
				Draw();
			} catch (InterruptedException ex) {}
		}
	}
	@Override
	public boolean onTouch(View v, MotionEvent event)
	{
		// Determine the button state to apply based on the MotionEvent action flag.
		// TODO: This will not work when Axis support is added. Make sure to refactor this when that time comes
		// The reason it won't work is that when moving an axis, you would get the event MotionEvent.ACTION_MOVE.
		int buttonState = (event.getAction() == MotionEvent.ACTION_DOWN) ? ButtonState.PRESSED : ButtonState.RELEASED;

		for (InputOverlayDrawableButton item : overlayButtons)
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
		for (InputOverlayDrawableJoystick item : overlayJoysticks)
		{
			item.TrackEvent(event);
			int[] axisIDs = item.getAxisIDs();
			float[] axises = item.getAxisValues();

			for (int a = 0; a < 4; ++a)
				NativeLibrary.onTouchAxisEvent(axisIDs[a], axises[a]);
		}
		_sema.release();

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
		final Bitmap bitmap = BitmapFactory.decodeResource(res, resId);
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
	private static InputOverlayDrawableJoystick initializeOverlayJoystick(Context context, int resOuter, int resInner, int axisUp, int axisDown, int axisLeft, int axisRight)
	{
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Initialize the InputOverlayDrawableButton.
		final Bitmap bitmapOuter = BitmapFactory.decodeResource(res, resOuter);
		final Bitmap bitmapInner = BitmapFactory.decodeResource(res, resInner);

		// Now set the bounds for the InputOverlayDrawableButton.
		// This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
		int outerSize = bitmapOuter.getWidth() + 256;
		int X = 0;
		int Y = 0;
		Rect outerRect = new Rect(X, Y, X + outerSize, Y + outerSize);
		Rect innerRect = new Rect(0, 0, outerSize / 4, outerSize / 4);

		final InputOverlayDrawableJoystick overlayDrawable
				= new InputOverlayDrawableJoystick(res,
					bitmapOuter, bitmapInner,
					outerRect, innerRect,
					axisUp, axisDown, axisLeft, axisRight);


		return overlayDrawable;
	}
	
}
