/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

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
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

import java.util.HashSet;
import java.util.Set;

/**
 * Draws the interactive input overlay on top of the
 * {@link SurfaceView} that is rendering emulation.
 */
public final class InputOverlay extends SurfaceView implements OnTouchListener
{
	private final Set<InputOverlayDrawableButton> overlayButtons = new HashSet<>();
	private final Set<InputOverlayDrawableDpad> overlayDpads = new HashSet<>();
	private final Set<InputOverlayDrawableJoystick> overlayJoysticks = new HashSet<>();

	private boolean mIsInEditMode = false;
	private InputOverlayDrawableButton mButtonBeingConfigured;
	private InputOverlayDrawableDpad mDpadBeingConfigured;
	private InputOverlayDrawableJoystick mJoystickBeingConfigured;

	private SharedPreferences mPreferences;

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
		// Determine the button size based on the smaller screen dimension.
		// This makes sure the buttons are the same size in both portrait and landscape.
		DisplayMetrics dm = context.getResources().getDisplayMetrics();
		int minDimension = Math.min(dm.widthPixels, dm.heightPixels);

		return Bitmap.createScaledBitmap(bitmap,
				(int)(minDimension * scale),
				(int)(minDimension * scale),
				true);
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

		mPreferences = PreferenceManager.getDefaultSharedPreferences(getContext());

		// Load the controls.
		refreshControls();

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
		super.draw(canvas);

		for (InputOverlayDrawableButton button : overlayButtons)
		{
			button.draw(canvas);
		}

		for (InputOverlayDrawableDpad dpad : overlayDpads)
		{
			dpad.draw(canvas);
		}

		for (InputOverlayDrawableJoystick joystick: overlayJoysticks)
		{
			joystick.draw(canvas);
		}
	}

	@Override
	public boolean onTouch(View v, MotionEvent event)
	{
		if (isInEditMode())
		{
			return onTouchWhileEditing(event);
		}

		int pointerIndex = event.getActionIndex();

		for (InputOverlayDrawableButton button : overlayButtons)
		{
			// Determine the button state to apply based on the MotionEvent action flag.
			switch (event.getAction() & MotionEvent.ACTION_MASK)
			{
			case MotionEvent.ACTION_DOWN:
			case MotionEvent.ACTION_POINTER_DOWN:
				// If a pointer enters the bounds of a button, press that button.
				if (button.getBounds().contains((int)event.getX(pointerIndex), (int)event.getY(pointerIndex)))
				{
					button.setPressedState(true);
					button.setTrackId(event.getPointerId(pointerIndex));
					NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, button.getId(), ButtonState.PRESSED);
				}
				break;
			case MotionEvent.ACTION_UP:
			case MotionEvent.ACTION_POINTER_UP:
				// If a pointer ends, release the button it was pressing.
				if (button.getTrackId() == event.getPointerId(pointerIndex))
				{
					button.setPressedState(false);
					NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, button.getId(), ButtonState.RELEASED);
				}
				break;
			}
		}

		for (InputOverlayDrawableDpad dpad : overlayDpads)
		{
			// Determine the button state to apply based on the MotionEvent action flag.
			switch (event.getAction() & MotionEvent.ACTION_MASK)
			{
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					// If a pointer enters the bounds of a button, press that button.
					if (dpad.getBounds().contains((int)event.getX(pointerIndex), (int)event.getY(pointerIndex)))
					{
						boolean up = false;
						boolean down = false;
						boolean left = false;
						boolean right = false;
						if (dpad.getBounds().top + (dpad.getHeight() / 3) > (int)event.getY(pointerIndex))
						{
							NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(0), ButtonState.PRESSED);
							up = true;
						}
						if (dpad.getBounds().bottom - (dpad.getHeight() / 3) < (int)event.getY(pointerIndex))
						{
							NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(1), ButtonState.PRESSED);
							down = true;
						}
						if (dpad.getBounds().left + (dpad.getWidth() / 3) > (int)event.getX(pointerIndex))
						{
							NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(2), ButtonState.PRESSED);
							left = true;
						}
						if (dpad.getBounds().right - (dpad.getWidth() / 3) < (int)event.getX(pointerIndex))
						{
							NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(3), ButtonState.PRESSED);
							right = true;
						}

						setDpadState(dpad, up, down, left, right);
						dpad.setTrackId(event.getPointerId(pointerIndex));
					}
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					// If a pointer ends, release the buttons.
					if (dpad.getTrackId() == event.getPointerId(pointerIndex))
					{
						for(int i = 0; i < 4; i++)
						{
							dpad.setState(InputOverlayDrawableDpad.STATE_DEFAULT);
							NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(i), ButtonState.RELEASED);
						}
					}
					break;
			}
		}

		for (InputOverlayDrawableJoystick joystick : overlayJoysticks)
		{
			joystick.TrackEvent(event);
			int[] axisIDs = joystick.getAxisIDs();
			float[] axises = joystick.getAxisValues();

			for (int i = 0; i < 4; i++)
			{
				NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, axisIDs[i], axises[i]);
			}
		}

		invalidate();

		return true;
	}

	public boolean onTouchWhileEditing(MotionEvent event)
	{
		int pointerIndex = event.getActionIndex();
		int fingerPositionX = (int)event.getX(pointerIndex);
		int fingerPositionY = (int)event.getY(pointerIndex);

		// Maybe combine Button and Joystick as subclasses of the same parent?
		// Or maybe create an interface like IMoveableHUDControl?

		for (InputOverlayDrawableButton button : overlayButtons)
		{
			// Determine the button state to apply based on the MotionEvent action flag.
			switch (event.getAction() & MotionEvent.ACTION_MASK)
			{
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					// If no button is being moved now, remember the currently touched button to move.
					if (mButtonBeingConfigured == null && button.getBounds().contains(fingerPositionX, fingerPositionY))
					{
						mButtonBeingConfigured = button;
						mButtonBeingConfigured.onConfigureTouch(event);
					}
					break;
				case MotionEvent.ACTION_MOVE:
					if (mButtonBeingConfigured != null)
					{
						mButtonBeingConfigured.onConfigureTouch(event);
						invalidate();
						return true;
					}
					break;

				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					if (mButtonBeingConfigured == button)
					{
						// Persist button position by saving new place.
						saveControlPosition(mButtonBeingConfigured.getId(), mButtonBeingConfigured.getBounds().left, mButtonBeingConfigured.getBounds().top);
						mButtonBeingConfigured = null;
					}
					break;
			}
		}

		for (InputOverlayDrawableDpad dpad : overlayDpads)
		{
			// Determine the button state to apply based on the MotionEvent action flag.
			switch (event.getAction() & MotionEvent.ACTION_MASK)
			{
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					// If no button is being moved now, remember the currently touched button to move.
					if (mButtonBeingConfigured == null && dpad.getBounds().contains(fingerPositionX, fingerPositionY))
					{
						mDpadBeingConfigured = dpad;
						mDpadBeingConfigured.onConfigureTouch(event);
					}
					break;
				case MotionEvent.ACTION_MOVE:
					if (mDpadBeingConfigured != null)
					{
						mDpadBeingConfigured.onConfigureTouch(event);
						invalidate();
						return true;
					}
					break;

				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					if (mDpadBeingConfigured == dpad)
					{
						// Persist button position by saving new place.
						saveControlPosition(mDpadBeingConfigured.getId(0), mDpadBeingConfigured.getBounds().left, mDpadBeingConfigured.getBounds().top);
						mDpadBeingConfigured = null;
					}
					break;
			}
		}

		for (InputOverlayDrawableJoystick joystick : overlayJoysticks)
		{
			switch (event.getAction())
			{
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					if (mJoystickBeingConfigured == null && joystick.getBounds().contains(fingerPositionX, fingerPositionY))
					{
						mJoystickBeingConfigured = joystick;
						mJoystickBeingConfigured.onConfigureTouch(event);
					}
					break;
				case MotionEvent.ACTION_MOVE:
					if (mJoystickBeingConfigured != null)
					{
						mJoystickBeingConfigured.onConfigureTouch(event);
						invalidate();
					}
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					if (mJoystickBeingConfigured != null)
					{
						saveControlPosition(mJoystickBeingConfigured.getId(), mJoystickBeingConfigured.getBounds().left, mJoystickBeingConfigured.getBounds().top);
						mJoystickBeingConfigured = null;
					}
					break;
			}
		}

		return true;
	}

	private void setDpadState(InputOverlayDrawableDpad dpad, boolean up, boolean down, boolean left, boolean right)
	{
		if (up)
		{
			if (left)
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_LEFT);
			else if (right)
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_RIGHT);
			else
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP);
		}
		else if (down)
		{
			if (left)
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_LEFT);
			else if (right)
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_RIGHT);
			else
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN);
		}
		else if (left)
		{
			dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_LEFT);
		}
		else if (right)
		{
			dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_RIGHT);
		}
	}

	private void addGameCubeOverlayControls()
	{
		if (mPreferences.getBoolean("buttonToggleGc0", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_a, R.drawable.gcpad_a_pressed, ButtonType.BUTTON_A));
		}
		if (mPreferences.getBoolean("buttonToggleGc1", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_b, R.drawable.gcpad_b_pressed, ButtonType.BUTTON_B));
		}
		if (mPreferences.getBoolean("buttonToggleGc2", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_x, R.drawable.gcpad_x_pressed, ButtonType.BUTTON_X));
		}
		if (mPreferences.getBoolean("buttonToggleGc3", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_y, R.drawable.gcpad_y_pressed, ButtonType.BUTTON_Y));
		}
		if (mPreferences.getBoolean("buttonToggleGc4", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_z, R.drawable.gcpad_z_pressed, ButtonType.BUTTON_Z));
		}
		if (mPreferences.getBoolean("buttonToggleGc5", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_start, R.drawable.gcpad_start_pressed, ButtonType.BUTTON_START));
		}
		if (mPreferences.getBoolean("buttonToggleGc6", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_l, R.drawable.gcpad_l_pressed, ButtonType.TRIGGER_L));
		}
		if (mPreferences.getBoolean("buttonToggleGc7", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_r, R.drawable.gcpad_r_pressed, ButtonType.TRIGGER_R));
		}
		if (mPreferences.getBoolean("buttonToggleGc8", true))
		{
			overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.gcwii_dpad,
					R.drawable.gcwii_dpad_pressed_one_direction, R.drawable.gcwii_dpad_pressed_two_directions,
					ButtonType.BUTTON_UP, ButtonType.BUTTON_DOWN,
					ButtonType.BUTTON_LEFT, ButtonType.BUTTON_RIGHT));
		}
		if (mPreferences.getBoolean("buttonToggleGc9", true))
		{
			overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
					R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.STICK_MAIN));
		}
		if (mPreferences.getBoolean("buttonToggleGc10", true))
		{
			overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
					R.drawable.gcpad_c, R.drawable.gcpad_c_pressed, ButtonType.STICK_C));
		}
	}

	private void addWiimoteOverlayControls()
	{
		if (mPreferences.getBoolean("buttonToggleWii0", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_a, R.drawable.wiimote_a_pressed, ButtonType.WIIMOTE_BUTTON_A));
		}
		if (mPreferences.getBoolean("buttonToggleWii1", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_b, R.drawable.wiimote_b_pressed, ButtonType.WIIMOTE_BUTTON_B));
		}
		if (mPreferences.getBoolean("buttonToggleWii2", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_one, R.drawable.wiimote_one_pressed, ButtonType.WIIMOTE_BUTTON_1));
		}
		if (mPreferences.getBoolean("buttonToggleWii3", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_two, R.drawable.wiimote_two_pressed, ButtonType.WIIMOTE_BUTTON_2));
		}
		if (mPreferences.getBoolean("buttonToggleWii4", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_plus, R.drawable.wiimote_plus_pressed, ButtonType.WIIMOTE_BUTTON_PLUS));
		}
		if (mPreferences.getBoolean("buttonToggleWii5", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_minus, R.drawable.wiimote_minus_pressed, ButtonType.WIIMOTE_BUTTON_MINUS));
		}
		if (mPreferences.getBoolean("buttonToggleWii6", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_home, R.drawable.wiimote_home_pressed, ButtonType.WIIMOTE_BUTTON_HOME));
		}
		if (mPreferences.getBoolean("buttonToggleWii7", true))
		{
			if (mPreferences.getInt("wiiController", 3) == 2)
			{
				overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.gcwii_dpad,
						R.drawable.gcwii_dpad_pressed_one_direction, R.drawable.gcwii_dpad_pressed_two_directions,
						ButtonType.WIIMOTE_RIGHT, ButtonType.WIIMOTE_LEFT,
						ButtonType.WIIMOTE_UP, ButtonType.WIIMOTE_DOWN));
			}
			else
			{
				overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.gcwii_dpad,
						R.drawable.gcwii_dpad_pressed_one_direction, R.drawable.gcwii_dpad_pressed_two_directions,
						ButtonType.WIIMOTE_UP, ButtonType.WIIMOTE_DOWN,
						ButtonType.WIIMOTE_LEFT, ButtonType.WIIMOTE_RIGHT));
			}
		}
	}

	private void addNunchukOverlayControls()
	{
		if (mPreferences.getBoolean("buttonToggleWii8", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.nunchuk_c, R.drawable.nunchuk_c_pressed, ButtonType.NUNCHUK_BUTTON_C));
		}
		if (mPreferences.getBoolean("buttonToggleWii9", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.nunchuk_z, R.drawable.nunchuk_z_pressed, ButtonType.NUNCHUK_BUTTON_Z));
		}
		if (mPreferences.getBoolean("buttonToggleWii10", true))
		{
			overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
					R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.NUNCHUK_STICK));
		}
	}

	private void addClassicOverlayControls()
	{
		if (mPreferences.getBoolean("buttonToggleClassic0", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_a, R.drawable.classic_a_pressed, ButtonType.CLASSIC_BUTTON_A));
		}
		if (mPreferences.getBoolean("buttonToggleClassic1", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_b, R.drawable.classic_b_pressed, ButtonType.CLASSIC_BUTTON_B));
		}
		if (mPreferences.getBoolean("buttonToggleClassic2", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_x, R.drawable.classic_x_pressed, ButtonType.CLASSIC_BUTTON_X));
		}
		if (mPreferences.getBoolean("buttonToggleClassic3", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_y, R.drawable.classic_y_pressed, ButtonType.CLASSIC_BUTTON_Y));
		}
		if (mPreferences.getBoolean("buttonToggleClassic4", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_plus, R.drawable.wiimote_plus_pressed, ButtonType.CLASSIC_BUTTON_PLUS));
		}
		if (mPreferences.getBoolean("buttonToggleClassic5", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_minus, R.drawable.wiimote_minus_pressed, ButtonType.CLASSIC_BUTTON_MINUS));
		}
		if (mPreferences.getBoolean("buttonToggleClassic6", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_home, R.drawable.wiimote_home_pressed, ButtonType.CLASSIC_BUTTON_HOME));
		}
		if (mPreferences.getBoolean("buttonToggleClassic7", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_l, R.drawable.classic_l_pressed, ButtonType.CLASSIC_TRIGGER_L));
		}
		if (mPreferences.getBoolean("buttonToggleClassic8", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_r, R.drawable.classic_r_pressed, ButtonType.CLASSIC_TRIGGER_R));
		}
		if (mPreferences.getBoolean("buttonToggleClassic9", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_zl, R.drawable.classic_zl_pressed, ButtonType.CLASSIC_BUTTON_ZL));
		}
		if (mPreferences.getBoolean("buttonToggleClassic10", true))
		{
			overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_zr, R.drawable.classic_zr_pressed, ButtonType.CLASSIC_BUTTON_ZR));
		}
		if (mPreferences.getBoolean("buttonToggleClassic11", true))
		{
			overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.gcwii_dpad,
					R.drawable.gcwii_dpad_pressed_one_direction, R.drawable.gcwii_dpad_pressed_two_directions,
					ButtonType.CLASSIC_DPAD_UP, ButtonType.CLASSIC_DPAD_DOWN,
					ButtonType.CLASSIC_DPAD_LEFT, ButtonType.CLASSIC_DPAD_RIGHT));
		}
		if (mPreferences.getBoolean("buttonToggleClassic12", true))
		{
			overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
					R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.CLASSIC_STICK_LEFT));
		}
		if (mPreferences.getBoolean("buttonToggleClassic13", true))
		{
			overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
					R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.CLASSIC_STICK_RIGHT));
		}
	}

	public void refreshControls()
	{
		// Remove all the overlay buttons from the HashSet.
		overlayButtons.removeAll(overlayButtons);
		overlayDpads.removeAll(overlayDpads);
		overlayJoysticks.removeAll(overlayJoysticks);

		// Add all the enabled overlay items back to the HashSet.
		if (EmulationActivity.isGameCubeGame() || mPreferences.getInt("wiiController", 3) == 0)
		{
			addGameCubeOverlayControls();
		}
		else if (mPreferences.getInt("wiiController", 3) == 4)
		{
			addClassicOverlayControls();
		}
		else
		{
			addWiimoteOverlayControls();
			if (mPreferences.getInt("wiiController", 3) == 3)
			{
				addNunchukOverlayControls();
			}
		}

		invalidate();
	}

	private void saveControlPosition(int sharedPrefsId, int x, int y)
	{
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(getContext());
		SharedPreferences.Editor sPrefsEditor = sPrefs.edit();
		sPrefsEditor.putFloat(sharedPrefsId+"-X", x);
		sPrefsEditor.putFloat(sharedPrefsId+"-Y", y);
		sPrefsEditor.apply();
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
	 * @param context      The current {@link Context}.
	 * @param defaultResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Default State).
	 * @param pressedResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Pressed State).
	 * @param buttonId     Identifier for determining what type of button the initialized InputOverlayDrawableButton represents.
	 *
	 * @return An {@link InputOverlayDrawableButton} with the correct drawing bounds set.
	 *
	 */
	private static InputOverlayDrawableButton initializeOverlayButton(Context context, int defaultResId, int pressedResId, int buttonId)
	{
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Decide scale based on button ID and user preference
		float scale;

		switch (buttonId)
		{
		case ButtonType.BUTTON_A:
		case ButtonType.WIIMOTE_BUTTON_B:
		case ButtonType.NUNCHUK_BUTTON_Z:
			scale = 0.2f;
			break;
		case ButtonType.BUTTON_X:
		case ButtonType.BUTTON_Y:
			scale = 0.175f;
			break;
		case ButtonType.BUTTON_Z:
		case ButtonType.TRIGGER_L:
		case ButtonType.TRIGGER_R:
			scale = 0.225f;
			break;
		case ButtonType.BUTTON_START:
			scale = 0.075f;
			break;
		case ButtonType.WIIMOTE_BUTTON_1:
		case ButtonType.WIIMOTE_BUTTON_2:
			scale = 0.0875f;
			break;
		case ButtonType.WIIMOTE_BUTTON_PLUS:
		case ButtonType.WIIMOTE_BUTTON_MINUS:
		case ButtonType.WIIMOTE_BUTTON_HOME:
		case ButtonType.CLASSIC_BUTTON_PLUS:
		case ButtonType.CLASSIC_BUTTON_MINUS:
		case ButtonType.CLASSIC_BUTTON_HOME:
			scale = 0.0625f;
			break;
		case ButtonType.CLASSIC_TRIGGER_L:
		case ButtonType.CLASSIC_TRIGGER_R:
		case ButtonType.CLASSIC_BUTTON_ZL:
		case ButtonType.CLASSIC_BUTTON_ZR:
			scale = 0.25f;
			break;
		default:
			scale = 0.125f;
			break;
		}

		scale *= (sPrefs.getInt("controlScale", 50) + 50);
		scale /= 100;

		// Initialize the InputOverlayDrawableButton.
		final Bitmap defaultStateBitmap = resizeBitmap(context, BitmapFactory.decodeResource(res, defaultResId), scale);
		final Bitmap pressedStateBitmap = resizeBitmap(context, BitmapFactory.decodeResource(res, pressedResId), scale);
		final InputOverlayDrawableButton overlayDrawable = new InputOverlayDrawableButton(res, defaultStateBitmap, pressedStateBitmap, buttonId);

		// The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
		// These were set in the input overlay configuration menu.
		int drawableX = (int) sPrefs.getFloat(buttonId + "-X", 0f);
		int drawableY = (int) sPrefs.getFloat(buttonId + "-Y", 0f);

		int width = overlayDrawable.getWidth();
		int height = overlayDrawable.getHeight();

		// Now set the bounds for the InputOverlayDrawableButton.
		// This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
		overlayDrawable.setBounds(drawableX, drawableY, drawableX + width, drawableY + height);

		// Need to set the image's position
		overlayDrawable.setPosition(drawableX, drawableY);

		return overlayDrawable;
	}

	/**
	 * Initializes an {@link InputOverlayDrawableDpad}
	 *
	 * @param context                   The current {@link Context}.
	 * @param defaultResId              The {@link Bitmap} resource ID of the default sate.
	 * @param pressedOneDirectionResId  The {@link Bitmap} resource ID of the pressed sate in one direction.
	 * @param pressedTwoDirectionsResId The {@link Bitmap} resource ID of the pressed sate in two directions.
	 * @param buttonUp                  Identifier for the up button.
	 * @param buttonDown                Identifier for the down button.
	 * @param buttonLeft                Identifier for the left button.
	 * @param buttonRight               Identifier for the right button.
	 *
	 * @return the initialized {@link InputOverlayDrawableDpad}
	 */
	private static InputOverlayDrawableDpad initializeOverlayDpad(Context context,
	                                                              int defaultResId,
	                                                              int pressedOneDirectionResId,
	                                                              int pressedTwoDirectionsResId,
	                                                              int buttonUp,
	                                                              int buttonDown,
	                                                              int buttonLeft,
	                                                              int buttonRight)
	{
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableDpad.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Decide scale based on button ID and user preference
		float scale;

		switch (buttonUp)
		{
		case ButtonType.BUTTON_UP:
			scale = 0.2375f;
			break;
		case ButtonType.CLASSIC_DPAD_UP:
			scale = 0.275f;
			break;
		default:
			scale = 0.2125f;
			break;
		}

		scale *= (sPrefs.getInt("controlScale", 50) + 50);
		scale /= 100;

		// Initialize the InputOverlayDrawableDpad.
		final Bitmap defaultStateBitmap = resizeBitmap(context, BitmapFactory.decodeResource(res, defaultResId), scale);
		final Bitmap pressedOneDirectionStateBitmap = resizeBitmap(context, BitmapFactory.decodeResource(res, pressedOneDirectionResId), scale);
		final Bitmap pressedTwoDirectionsStateBitmap = resizeBitmap(context, BitmapFactory.decodeResource(res, pressedTwoDirectionsResId), scale);
		final InputOverlayDrawableDpad overlayDrawable = new InputOverlayDrawableDpad(res, defaultStateBitmap,
				pressedOneDirectionStateBitmap, pressedTwoDirectionsStateBitmap,
				buttonUp, buttonDown, buttonLeft, buttonRight);

		// The X and Y coordinates of the InputOverlayDrawableDpad on the InputOverlay.
		// These were set in the input overlay configuration menu.
		int drawableX = (int) sPrefs.getFloat(buttonUp + "-X", 0f);
		int drawableY = (int) sPrefs.getFloat(buttonUp + "-Y", 0f);

		int width = overlayDrawable.getWidth();
		int height = overlayDrawable.getHeight();

		// Now set the bounds for the InputOverlayDrawableDpad.
		// This will dictate where on the screen (and the what the size) the InputOverlayDrawableDpad will be.
		overlayDrawable.setBounds(drawableX, drawableY, drawableX + width, drawableY + height);

		// Need to set the image's position
		overlayDrawable.setPosition(drawableX, drawableY);

		return overlayDrawable;
	}

	/**
	 * Initializes an {@link InputOverlayDrawableJoystick}
	 *
	 * @param context         The current {@link Context}
	 * @param resOuter        Resource ID for the outer image of the joystick (the static image that shows the circular bounds).
	 * @param defaultResInner Resource ID for the default inner image of the joystick (the one you actually move around).
	 * @param pressedResInner Resource ID for the pressed inner image of the joystick.
	 * @param joystick        Identifier for which joystick this is.
	 *
	 * @return the initialized {@link InputOverlayDrawableJoystick}.
	 */
	private static InputOverlayDrawableJoystick initializeOverlayJoystick(Context context, int resOuter, int defaultResInner, int pressedResInner, int joystick)
	{
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableJoystick.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

		// Decide scale based on user preference
		float scale = 0.275f;
		scale *= (sPrefs.getInt("controlScale", 50) + 50);
		scale /= 100;

		// Initialize the InputOverlayDrawableJoystick.
		final Bitmap bitmapOuter = resizeBitmap(context, BitmapFactory.decodeResource(res, resOuter), scale);
		final Bitmap bitmapInnerDefault = BitmapFactory.decodeResource(res, defaultResInner);
		final Bitmap bitmapInnerPressed = BitmapFactory.decodeResource(res, pressedResInner);

		// The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
		// These were set in the input overlay configuration menu.
		int drawableX = (int) sPrefs.getFloat(joystick + "-X", 0f);
		int drawableY = (int) sPrefs.getFloat(joystick + "-Y", 0f);

		// Decide inner scale based on joystick ID
		float innerScale;

		switch (joystick)
		{
		case ButtonType.STICK_C:
			innerScale = 1.833f;
			break;
		default:
			innerScale = 1.375f;
			break;
		}

		// Now set the bounds for the InputOverlayDrawableJoystick.
		// This will dictate where on the screen (and the what the size) the InputOverlayDrawableJoystick will be.
		int outerSize = bitmapOuter.getWidth();
		Rect outerRect = new Rect(drawableX, drawableY, drawableX + outerSize, drawableY + outerSize);
		Rect innerRect = new Rect(0, 0, (int) (outerSize / innerScale), (int) (outerSize / innerScale));

		// Send the drawableId to the joystick so it can be referenced when saving control position.
		final InputOverlayDrawableJoystick overlayDrawable
				= new InputOverlayDrawableJoystick(res, bitmapOuter,
								   bitmapInnerDefault, bitmapInnerPressed,
								   outerRect, innerRect, joystick);

		// Need to set the image's position
		overlayDrawable.setPosition(drawableX, drawableY);

		return overlayDrawable;
	}

	public void setIsInEditMode(boolean isInEditMode)
	{
		mIsInEditMode = isInEditMode;
	}

	public boolean isInEditMode()
	{
		return mIsInEditMode;
	}
}
