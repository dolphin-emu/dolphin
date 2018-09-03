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
import android.util.Log;
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
public final class InputOverlay extends SurfaceView implements OnTouchListener {
	public static final String CONTROL_INIT_PREF_KEY = "InitOverlay";
	public static final String CONTROL_SCALE_PREF_KEY = "ControlScale";

	public static final String CONTROL_TYPE_PREF_KEY = "WiiController";
	public static final int CONTROLLER_GAMECUBE = 0;
	public static final int COCONTROLLER_CLASSIC = 1;
	public static final int CONTROLLER_WIINUNCHUK = 2;
	public static final int CONTROLLER_WIIREMOTE = 3;

	public static final String JOYSTICK_PREF_KEY = "JoystickSetting";
	public static final int JOYSTICK_RELATIVE_CENTER = 0;
	public static final int JOYSTICK_FIXED_CENTER = 1;
	public static final int JOYSTICK_EMULATE_IR = 2;
	public static int JoyStickSetting;

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
	 * @return The scaled {@link Bitmap}
	 */
	public static Bitmap resizeBitmap(Context context, Bitmap bitmap, float scale) {
		// Determine the button size based on the smaller screen dimension.
		// This makes sure the buttons are the same size in both portrait and landscape.
		DisplayMetrics dm = context.getResources().getDisplayMetrics();
		int minDimension = Math.min(dm.widthPixels, dm.heightPixels);

		return Bitmap.createScaledBitmap(bitmap,
			(int) (minDimension * scale),
			(int) (minDimension * scale),
			true);
	}

	/**
	 * Constructor
	 *
	 * @param context The current {@link Context}.
	 * @param attrs   {@link AttributeSet} for parsing XML attributes.
	 */
	public InputOverlay(Context context, AttributeSet attrs) {
		super(context, attrs);

		mPreferences = PreferenceManager.getDefaultSharedPreferences(getContext());
		if (!mPreferences.getBoolean(CONTROL_INIT_PREF_KEY, false))
			defaultOverlay();

		JoyStickSetting = mPreferences.getInt(InputOverlay.JOYSTICK_PREF_KEY,
			InputOverlay.JOYSTICK_RELATIVE_CENTER);

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
	public void draw(Canvas canvas) {
		super.draw(canvas);

		for (InputOverlayDrawableButton button : overlayButtons) {
			button.draw(canvas);
		}

		for (InputOverlayDrawableDpad dpad : overlayDpads) {
			dpad.draw(canvas);
		}

		for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
			joystick.draw(canvas);
		}
	}

	@Override
	public boolean onTouch(View v, MotionEvent event) {
		if (isInEditMode()) {
			return onTouchWhileEditing(event);
		}

		int pointerIndex = event.getActionIndex();

		for (InputOverlayDrawableButton button : overlayButtons) {
			// Determine the button state to apply based on the MotionEvent action flag.
			switch (event.getAction() & MotionEvent.ACTION_MASK) {
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					// If a pointer enters the bounds of a button, press that button.
					if (button.getBounds().contains((int) event.getX(pointerIndex), (int) event.getY(pointerIndex))) {
						button.setPressedState(true);
						button.setTrackId(event.getPointerId(pointerIndex));
						NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, button.getId(), ButtonState.PRESSED);
					}
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					// If a pointer ends, release the button it was pressing.
					if (button.getTrackId() == event.getPointerId(pointerIndex)) {
						button.setPressedState(false);
						NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, button.getId(), ButtonState.RELEASED);
					}
					break;
			}
		}

		for (InputOverlayDrawableDpad dpad : overlayDpads) {
			// Determine the button state to apply based on the MotionEvent action flag.
			switch (event.getAction() & MotionEvent.ACTION_MASK) {
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
				case MotionEvent.ACTION_MOVE:
					// Up, Down, Left, Right
					boolean[] pressed = {false, false, false, false};
					// If a pointer enters the bounds of a button, press that button.
					if (dpad.getBounds().contains((int) event.getX(pointerIndex), (int) event.getY(pointerIndex))) {
						if (dpad.getBounds().top + (dpad.getHeight() / 3) > (int) event.getY(pointerIndex))
							pressed[0] = true;
						if (dpad.getBounds().bottom - (dpad.getHeight() / 3) < (int) event.getY(pointerIndex))
							pressed[1] = true;
						if (dpad.getBounds().left + (dpad.getWidth() / 3) > (int) event.getX(pointerIndex))
							pressed[2] = true;
						if (dpad.getBounds().right - (dpad.getWidth() / 3) < (int) event.getX(pointerIndex))
							pressed[3] = true;

						// Release the buttons first, then press
						for (int i = 0; i < pressed.length; i++)
							if (!pressed[i])
								NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(i), ButtonState.RELEASED);
						// Press buttons
						for (int i = 0; i < pressed.length; i++)
							if (pressed[i])
								NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(i), ButtonState.PRESSED);

						setDpadState(dpad, pressed[0], pressed[1], pressed[2], pressed[3]);
						dpad.setTrackId(event.getPointerId(pointerIndex));
					}
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					// If a pointer ends, release the buttons.
					if (dpad.getTrackId() == event.getPointerId(pointerIndex)) {
						for (int i = 0; i < 4; i++) {
							dpad.setState(InputOverlayDrawableDpad.STATE_DEFAULT);
							NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(i), ButtonState.RELEASED);
						}
					}
					break;
			}
		}

		for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
			joystick.TrackEvent(event);
			int[] axisIDs = joystick.getAxisIDs();
			float[] axises = joystick.getAxisValues();

			if(axisIDs[0] == ButtonType.NUNCHUK_STICK_UP && JoyStickSetting == JOYSTICK_EMULATE_IR) {
				int[] IRIDs = {
					ButtonType.WIIMOTE_IR+1,
					ButtonType.WIIMOTE_IR+2,
					ButtonType.WIIMOTE_IR+3,
					ButtonType.WIIMOTE_IR+4
				};
				for (int i = 0; i < 4; i++) {
					NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, IRIDs[i], -axises[i]);
				}
			}
			else {
				for (int i = 0; i < 4; i++) {
					NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, axisIDs[i], axises[i]);
				}
			}
		}

		invalidate();

		return true;
	}

	public boolean onTouchWhileEditing(MotionEvent event) {
		int pointerIndex = event.getActionIndex();
		int fingerPositionX = (int) event.getX(pointerIndex);
		int fingerPositionY = (int) event.getY(pointerIndex);

		// Maybe combine Button and Joystick as subclasses of the same parent?
		// Or maybe create an interface like IMoveableHUDControl?

		for (InputOverlayDrawableButton button : overlayButtons) {
			// Determine the button state to apply based on the MotionEvent action flag.
			switch (event.getAction() & MotionEvent.ACTION_MASK) {
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					// If no button is being moved now, remember the currently touched button to move.
					if (mButtonBeingConfigured == null && button.getBounds().contains(fingerPositionX, fingerPositionY)) {
						mButtonBeingConfigured = button;
						mButtonBeingConfigured.onConfigureTouch(event);
					}
					break;
				case MotionEvent.ACTION_MOVE:
					if (mButtonBeingConfigured != null) {
						mButtonBeingConfigured.onConfigureTouch(event);
						invalidate();
						return true;
					}
					break;

				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					if (mButtonBeingConfigured == button) {
						// Persist button position by saving new place.
						saveControlPosition(mButtonBeingConfigured.getId(), mButtonBeingConfigured.getBounds());
						mButtonBeingConfigured = null;
					}
					break;
			}
		}

		for (InputOverlayDrawableDpad dpad : overlayDpads) {
			// Determine the button state to apply based on the MotionEvent action flag.
			switch (event.getAction() & MotionEvent.ACTION_MASK) {
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					// If no button is being moved now, remember the currently touched button to move.
					if (mButtonBeingConfigured == null && dpad.getBounds().contains(fingerPositionX, fingerPositionY)) {
						mDpadBeingConfigured = dpad;
						mDpadBeingConfigured.onConfigureTouch(event);
					}
					break;
				case MotionEvent.ACTION_MOVE:
					if (mDpadBeingConfigured != null) {
						mDpadBeingConfigured.onConfigureTouch(event);
						invalidate();
						return true;
					}
					break;

				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					if (mDpadBeingConfigured == dpad) {
						// Persist button position by saving new place.
						saveControlPosition(mDpadBeingConfigured.getId(0), mDpadBeingConfigured.getBounds());
						mDpadBeingConfigured = null;
					}
					break;
			}
		}

		for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
			switch (event.getAction()) {
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
					if (mJoystickBeingConfigured == null && joystick.getBounds().contains(fingerPositionX, fingerPositionY)) {
						mJoystickBeingConfigured = joystick;
						mJoystickBeingConfigured.onConfigureTouch(event);
					}
					break;
				case MotionEvent.ACTION_MOVE:
					if (mJoystickBeingConfigured != null) {
						mJoystickBeingConfigured.onConfigureTouch(event);
						invalidate();
					}
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					if (mJoystickBeingConfigured != null) {
						saveControlPosition(mJoystickBeingConfigured.getId(), mJoystickBeingConfigured.getBounds());
						mJoystickBeingConfigured = null;
					}
					break;
			}
		}

		return true;
	}

	private void setDpadState(InputOverlayDrawableDpad dpad, boolean up, boolean down, boolean left, boolean right) {
		if (up) {
			if (left)
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_LEFT);
			else if (right)
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_RIGHT);
			else
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP);
		} else if (down) {
			if (left)
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_LEFT);
			else if (right)
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_RIGHT);
			else
				dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN);
		} else if (left) {
			dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_LEFT);
		} else if (right) {
			dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_RIGHT);
		}
	}

	private void addGameCubeOverlayControls() {
		if (mPreferences.getBoolean("buttonToggleGc0", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.gcpad_a, R.drawable.gcpad_a_pressed, ButtonType.BUTTON_A));
		}
		if (mPreferences.getBoolean("buttonToggleGc1", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.gcpad_b, R.drawable.gcpad_b_pressed, ButtonType.BUTTON_B));
		}
		if (mPreferences.getBoolean("buttonToggleGc2", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.gcpad_x, R.drawable.gcpad_x_pressed, ButtonType.BUTTON_X));
		}
		if (mPreferences.getBoolean("buttonToggleGc3", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.gcpad_y, R.drawable.gcpad_y_pressed, ButtonType.BUTTON_Y));
		}
		if (mPreferences.getBoolean("buttonToggleGc4", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.gcpad_z, R.drawable.gcpad_z_pressed, ButtonType.BUTTON_Z));
		}
		if (mPreferences.getBoolean("buttonToggleGc5", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.gcpad_start, R.drawable.gcpad_start_pressed, ButtonType.BUTTON_START));
		}
		if (mPreferences.getBoolean("buttonToggleGc6", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.gcpad_l, R.drawable.gcpad_l_pressed, ButtonType.TRIGGER_L));
		}
		if (mPreferences.getBoolean("buttonToggleGc7", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.gcpad_r, R.drawable.gcpad_r_pressed, ButtonType.TRIGGER_R));
		}
		if (mPreferences.getBoolean("buttonToggleGc8", true)) {
			overlayDpads.add(initializeOverlayDpad(R.drawable.gcwii_dpad,
				R.drawable.gcwii_dpad_pressed_one_direction, R.drawable.gcwii_dpad_pressed_two_directions,
				ButtonType.BUTTON_UP, ButtonType.BUTTON_DOWN,
				ButtonType.BUTTON_LEFT, ButtonType.BUTTON_RIGHT));
		}
		if (mPreferences.getBoolean("buttonToggleGc9", true)) {
			overlayJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick_range,
				R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.STICK_MAIN));
		}
		if (mPreferences.getBoolean("buttonToggleGc10", true)) {
			overlayJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick_range,
				R.drawable.gcpad_c, R.drawable.gcpad_c_pressed, ButtonType.STICK_C));
		}
	}

	private void addWiimoteOverlayControls() {
		if (mPreferences.getBoolean("buttonToggleWii0", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_a, R.drawable.wiimote_a_pressed, ButtonType.WIIMOTE_BUTTON_A));
		}
		if (mPreferences.getBoolean("buttonToggleWii1", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_b, R.drawable.wiimote_b_pressed, ButtonType.WIIMOTE_BUTTON_B));
		}
		if (mPreferences.getBoolean("buttonToggleWii2", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_one, R.drawable.wiimote_one_pressed, ButtonType.WIIMOTE_BUTTON_1));
		}
		if (mPreferences.getBoolean("buttonToggleWii3", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_two, R.drawable.wiimote_two_pressed, ButtonType.WIIMOTE_BUTTON_2));
		}
		if (mPreferences.getBoolean("buttonToggleWii4", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_plus, R.drawable.wiimote_plus_pressed, ButtonType.WIIMOTE_BUTTON_PLUS));
		}
		if (mPreferences.getBoolean("buttonToggleWii5", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_minus, R.drawable.wiimote_minus_pressed, ButtonType.WIIMOTE_BUTTON_MINUS));
		}
		if (mPreferences.getBoolean("buttonToggleWii6", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_home, R.drawable.wiimote_home_pressed, ButtonType.WIIMOTE_BUTTON_HOME));
		}
		if (mPreferences.getBoolean("buttonToggleWii7", true)) {
			if (mPreferences.getInt(CONTROL_TYPE_PREF_KEY, CONTROLLER_WIINUNCHUK) == CONTROLLER_WIINUNCHUK)
			{
				overlayDpads.add(initializeOverlayDpad(R.drawable.gcwii_dpad,
					R.drawable.gcwii_dpad_pressed_one_direction, R.drawable.gcwii_dpad_pressed_two_directions,
					ButtonType.WIIMOTE_UP, ButtonType.WIIMOTE_DOWN,
					ButtonType.WIIMOTE_LEFT, ButtonType.WIIMOTE_RIGHT));
			}
			else
			{
				// Horizontal Wii Remote
				overlayDpads.add(initializeOverlayDpad(R.drawable.gcwii_dpad,
					R.drawable.gcwii_dpad_pressed_one_direction, R.drawable.gcwii_dpad_pressed_two_directions,
					ButtonType.WIIMOTE_RIGHT, ButtonType.WIIMOTE_LEFT,
					ButtonType.WIIMOTE_UP, ButtonType.WIIMOTE_DOWN));
			}
		}
	}

	private void addNunchukOverlayControls() {
		if (mPreferences.getBoolean("buttonToggleWii8", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.nunchuk_c, R.drawable.nunchuk_c_pressed, ButtonType.NUNCHUK_BUTTON_C));
		}
		if (mPreferences.getBoolean("buttonToggleWii9", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.nunchuk_z, R.drawable.nunchuk_z_pressed, ButtonType.NUNCHUK_BUTTON_Z));
		}
		if (mPreferences.getBoolean("buttonToggleWii10", true)) {
			overlayJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick_range,
				R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.NUNCHUK_STICK));
		}
	}

	private void addClassicOverlayControls() {
		if (mPreferences.getBoolean("buttonToggleClassic0", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.classic_a, R.drawable.classic_a_pressed, ButtonType.CLASSIC_BUTTON_A));
		}
		if (mPreferences.getBoolean("buttonToggleClassic1", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.classic_b, R.drawable.classic_b_pressed, ButtonType.CLASSIC_BUTTON_B));
		}
		if (mPreferences.getBoolean("buttonToggleClassic2", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.classic_x, R.drawable.classic_x_pressed, ButtonType.CLASSIC_BUTTON_X));
		}
		if (mPreferences.getBoolean("buttonToggleClassic3", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.classic_y, R.drawable.classic_y_pressed, ButtonType.CLASSIC_BUTTON_Y));
		}
		if (mPreferences.getBoolean("buttonToggleClassic4", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_plus, R.drawable.wiimote_plus_pressed, ButtonType.CLASSIC_BUTTON_PLUS));
		}
		if (mPreferences.getBoolean("buttonToggleClassic5", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_minus, R.drawable.wiimote_minus_pressed, ButtonType.CLASSIC_BUTTON_MINUS));
		}
		if (mPreferences.getBoolean("buttonToggleClassic6", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.wiimote_home, R.drawable.wiimote_home_pressed, ButtonType.CLASSIC_BUTTON_HOME));
		}
		if (mPreferences.getBoolean("buttonToggleClassic7", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.classic_l, R.drawable.classic_l_pressed, ButtonType.CLASSIC_TRIGGER_L));
		}
		if (mPreferences.getBoolean("buttonToggleClassic8", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.classic_r, R.drawable.classic_r_pressed, ButtonType.CLASSIC_TRIGGER_R));
		}
		if (mPreferences.getBoolean("buttonToggleClassic9", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.classic_zl, R.drawable.classic_zl_pressed, ButtonType.CLASSIC_BUTTON_ZL));
		}
		if (mPreferences.getBoolean("buttonToggleClassic10", true)) {
			overlayButtons.add(initializeOverlayButton(R.drawable.classic_zr, R.drawable.classic_zr_pressed, ButtonType.CLASSIC_BUTTON_ZR));
		}
		if (mPreferences.getBoolean("buttonToggleClassic11", true)) {
			overlayDpads.add(initializeOverlayDpad(R.drawable.gcwii_dpad,
				R.drawable.gcwii_dpad_pressed_one_direction, R.drawable.gcwii_dpad_pressed_two_directions,
				ButtonType.CLASSIC_DPAD_UP, ButtonType.CLASSIC_DPAD_DOWN,
				ButtonType.CLASSIC_DPAD_LEFT, ButtonType.CLASSIC_DPAD_RIGHT));
		}
		if (mPreferences.getBoolean("buttonToggleClassic12", true)) {
			overlayJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick_range,
				R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.CLASSIC_STICK_LEFT));
		}
		if (mPreferences.getBoolean("buttonToggleClassic13", true)) {
			overlayJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick_range,
				R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.CLASSIC_STICK_RIGHT));
		}
	}

	public void refreshControls() {
		final int controller = mPreferences.getInt(CONTROL_TYPE_PREF_KEY, CONTROLLER_WIINUNCHUK);

		// Remove all the overlay buttons from the HashSet.
		overlayButtons.removeAll(overlayButtons);
		overlayDpads.removeAll(overlayDpads);
		overlayJoysticks.removeAll(overlayJoysticks);

		// Add all the enabled overlay items back to the HashSet.
		if (EmulationActivity.isGameCubeGame() || controller == CONTROLLER_GAMECUBE) {
			addGameCubeOverlayControls();
		} else if (controller == COCONTROLLER_CLASSIC) {
			addClassicOverlayControls();
		} else {
			addWiimoteOverlayControls();
			if(controller == CONTROLLER_WIINUNCHUK) {
				addNunchukOverlayControls();
			}
		}

		invalidate();
	}

	private void saveControlPosition(int buttonId, Rect bounds) {
		final Context context = getContext();
		final DisplayMetrics dm = context.getResources().getDisplayMetrics();
		final int controller = mPreferences.getInt(CONTROL_TYPE_PREF_KEY, CONTROLLER_WIINUNCHUK);
		SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
		float x = (bounds.left + (bounds.right - bounds.left) / 2.0f) / dm.widthPixels * 2.0f - 1.0f;
		float y = (bounds.top + (bounds.bottom - bounds.top) / 2.0f) / dm.heightPixels * 2.0f - 1.0f;
		sPrefsEditor.putFloat(controller + "_" + buttonId + "_X", x);
		sPrefsEditor.putFloat(controller + "_" + buttonId + "_Y", y);
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
	 * <li>No native calls</li>
	 * <li>Keeps Android-only values inside the Android environment</li>
	 * </ul>
	 * <p>
	 * Technically no modifications should need to be performed on the returned
	 * InputOverlayDrawableButton. Simply add it to the HashSet of overlay items and wait
	 * for Android to call the onDraw method.
	 *
	 * @param defaultResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Default State).
	 * @param pressedResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Pressed State).
	 * @param buttonId     Identifier for determining what type of button the initialized InputOverlayDrawableButton represents.
	 * @return An {@link InputOverlayDrawableButton} with the correct drawing bounds set.
	 */
	private InputOverlayDrawableButton initializeOverlayButton(int defaultResId, int pressedResId, int buttonId) {
		final Context context = getContext();
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();
		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
		final int controller = EmulationActivity.isGameCubeGame() ?
			CONTROLLER_GAMECUBE : mPreferences.getInt(CONTROL_TYPE_PREF_KEY, CONTROLLER_WIINUNCHUK);

		// Decide scale based on button ID and user preference
		float scale;

		switch (buttonId) {
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

		scale *= (mPreferences.getInt(CONTROL_SCALE_PREF_KEY, 50) + 50);
		scale /= 100;

		// Initialize the InputOverlayDrawableButton.
		final Bitmap defaultStateBitmap = resizeBitmap(context, BitmapFactory.decodeResource(res, defaultResId), scale);
		final Bitmap pressedStateBitmap = resizeBitmap(context, BitmapFactory.decodeResource(res, pressedResId), scale);
		final InputOverlayDrawableButton overlayDrawable = new InputOverlayDrawableButton(res, defaultStateBitmap, pressedStateBitmap, buttonId);

		// The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
		// These were set in the input overlay configuration menu.
		float x = mPreferences.getFloat(controller + "_" + buttonId + "_X", 0f);
		float y = mPreferences.getFloat(controller + "_" + buttonId + "_Y", 0f);

		int width = overlayDrawable.getWidth();
		int height = overlayDrawable.getHeight();
		final DisplayMetrics dm = res.getDisplayMetrics();
		int drawableX = (int) ((dm.widthPixels / 2.0f) * (1.0f + x) - width / 2.0f);
		int drawableY = (int) ((dm.heightPixels / 2.0f) * (1.0f + y) - height / 2.0f);

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
	 * @param defaultResId              The {@link Bitmap} resource ID of the default sate.
	 * @param pressedOneDirectionResId  The {@link Bitmap} resource ID of the pressed sate in one direction.
	 * @param pressedTwoDirectionsResId The {@link Bitmap} resource ID of the pressed sate in two directions.
	 * @param buttonUp                  Identifier for the up button.
	 * @param buttonDown                Identifier for the down button.
	 * @param buttonLeft                Identifier for the left button.
	 * @param buttonRight               Identifier for the right button.
	 * @return the initialized {@link InputOverlayDrawableDpad}
	 */
	private InputOverlayDrawableDpad initializeOverlayDpad(int defaultResId,
																																int pressedOneDirectionResId,
																																int pressedTwoDirectionsResId,
																																int buttonUp,
																																int buttonDown,
																																int buttonLeft,
																																int buttonRight) {
		final Context context = getContext();
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();
		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableDpad.
		final int controller = EmulationActivity.isGameCubeGame() ?
			CONTROLLER_GAMECUBE : mPreferences.getInt(CONTROL_TYPE_PREF_KEY, CONTROLLER_WIINUNCHUK);

		// Decide scale based on button ID and user preference
		float scale;

		switch (buttonUp) {
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

		scale *= (mPreferences.getInt(CONTROL_SCALE_PREF_KEY, 50) + 50);
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
		float x = mPreferences.getFloat(controller + "_" + buttonUp + "_X", 0f);
		float y = mPreferences.getFloat(controller + "_" + buttonUp + "_Y", 0f);

		int width = overlayDrawable.getWidth();
		int height = overlayDrawable.getHeight();
		final DisplayMetrics dm = res.getDisplayMetrics();
		int drawableX = (int) ((dm.widthPixels / 2.0f) * (1.0f + x) - width / 2.0f);
		int drawableY = (int) ((dm.heightPixels / 2.0f) * (1.0f + y) - height / 2.0f);

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
	 * @param resOuter        Resource ID for the outer image of the joystick (the static image that shows the circular bounds).
	 * @param defaultResInner Resource ID for the default inner image of the joystick (the one you actually move around).
	 * @param pressedResInner Resource ID for the pressed inner image of the joystick.
	 * @param joystick        Identifier for which joystick this is.
	 * @return the initialized {@link InputOverlayDrawableJoystick}.
	 */
	private InputOverlayDrawableJoystick initializeOverlayJoystick(int resOuter, int defaultResInner, int pressedResInner, int joystick) {
		final Context context = getContext();
		// Resources handle for fetching the initial Drawable resource.
		final Resources res = context.getResources();

		// SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableJoystick.
		final int controller = EmulationActivity.isGameCubeGame() ?
			CONTROLLER_GAMECUBE : mPreferences.getInt(CONTROL_TYPE_PREF_KEY, CONTROLLER_WIINUNCHUK);

		// Decide scale based on user preference
		float scale = 0.275f;
		scale *= (mPreferences.getInt(CONTROL_SCALE_PREF_KEY, 50) + 50);
		scale /= 100;

		// Initialize the InputOverlayDrawableJoystick.
		final Bitmap bitmapOuter = resizeBitmap(context, BitmapFactory.decodeResource(res, resOuter), scale);
		final Bitmap bitmapInnerDefault = BitmapFactory.decodeResource(res, defaultResInner);
		final Bitmap bitmapInnerPressed = BitmapFactory.decodeResource(res, pressedResInner);

		// The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
		// These were set in the input overlay configuration menu.
		float x = mPreferences.getFloat(controller + "_" + joystick + "_X", 0f);
		float y = mPreferences.getFloat(controller + "_" + joystick + "_Y", 0f);

		// Decide inner scale based on joystick ID
		float innerScale;

		switch (joystick) {
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
		final DisplayMetrics dm = res.getDisplayMetrics();
		int drawableX = (int) ((dm.widthPixels / 2.0f) * (1.0f + x) - outerSize / 2.0f);
		int drawableY = (int) ((dm.heightPixels / 2.0f) * (1.0f + y) - outerSize / 2.0f);

		Rect outerRect = new Rect(drawableX, drawableY, drawableX + outerSize, drawableY + outerSize);
		Rect innerRect = new Rect(0, 0, (int) (outerSize / innerScale), (int) (outerSize / innerScale));

		// Send the drawableId to the joystick so it can be referenced when saving control position.
		final InputOverlayDrawableJoystick overlayDrawable = new InputOverlayDrawableJoystick(
			res, bitmapOuter, bitmapInnerDefault, bitmapInnerPressed, outerRect, innerRect, joystick);

		// Need to set the image's position
		overlayDrawable.setPosition(drawableX, drawableY);

		return overlayDrawable;
	}

	public void setIsInEditMode(boolean isInEditMode) {
		mIsInEditMode = isInEditMode;
	}

	public boolean isInEditMode() {
		return mIsInEditMode;
	}

	private void defaultOverlay() {
		// It's possible that a user has created their overlay before this was added
		// Only change the overlay if the 'A' button is not in the upper corner.

		// GameCube
		if (mPreferences.getFloat(CONTROLLER_GAMECUBE + "_" + ButtonType.BUTTON_A + "_X", 0f) == 0f) {
			gcDefaultOverlay();
		}

		// Wii Nunchuk
		if (mPreferences.getFloat(CONTROLLER_WIINUNCHUK + "_" + ButtonType.WIIMOTE_BUTTON_A + "_X", 0f) == 0f) {
			wiiNunchukDefaultOverlay();
		}

		// Wii Remote
		if (mPreferences.getFloat(CONTROLLER_WIIREMOTE + "_" + ButtonType.WIIMOTE_BUTTON_A + "_X", 0f) == 0f) {
			wiiRemoteDefaultOverlay();
		}

		// Wii Classic
		if (mPreferences.getFloat(COCONTROLLER_CLASSIC + "_" + ButtonType.CLASSIC_BUTTON_A + "_X", 0f) == 0f) {
			wiiClassicDefaultOverlay();
		}

		SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
		sPrefsEditor.putBoolean(CONTROL_INIT_PREF_KEY, true);
		sPrefsEditor.apply();
	}

	private void gcDefaultOverlay() {
		final int controller = CONTROLLER_GAMECUBE;
		SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
		Resources res = getResources();

		// Each value is a percent from max X/Y stored as an int. Have to bring that value down
		// to a decimal before multiplying by MAX X/Y.
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_A + "_X", res.getInteger(R.integer.BUTTON_A_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_A + "_Y", res.getInteger(R.integer.BUTTON_A_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_B + "_X", res.getInteger(R.integer.BUTTON_B_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_B + "_Y", res.getInteger(R.integer.BUTTON_B_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_X + "_X", res.getInteger(R.integer.BUTTON_X_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_X + "_Y", res.getInteger(R.integer.BUTTON_X_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_Y + "_X", res.getInteger(R.integer.BUTTON_Y_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_Y + "_Y", res.getInteger(R.integer.BUTTON_Y_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_Z + "_X", res.getInteger(R.integer.BUTTON_Z_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_Z + "_Y", res.getInteger(R.integer.BUTTON_Z_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_UP + "_X", res.getInteger(R.integer.BUTTON_UP_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_UP + "_Y", res.getInteger(R.integer.BUTTON_UP_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.TRIGGER_L + "_X", res.getInteger(R.integer.TRIGGER_L_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.TRIGGER_L + "_Y", res.getInteger(R.integer.TRIGGER_L_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.TRIGGER_R + "_X", res.getInteger(R.integer.TRIGGER_R_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.TRIGGER_R + "_Y", res.getInteger(R.integer.TRIGGER_R_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_START + "_X", res.getInteger(R.integer.BUTTON_START_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.BUTTON_START + "_Y", res.getInteger(R.integer.BUTTON_START_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.STICK_C + "_X", res.getInteger(R.integer.STICK_C_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.STICK_C + "_Y", res.getInteger(R.integer.STICK_C_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.STICK_MAIN + "_X", res.getInteger(R.integer.STICK_MAIN_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.STICK_MAIN + "_Y", res.getInteger(R.integer.STICK_MAIN_Y) / 100.0f);

		// We want to commit right away, otherwise the overlay could load before this is saved.
		sPrefsEditor.commit();
	}

	private void wiiNunchukDefaultOverlay() {
		final int controller = CONTROLLER_WIINUNCHUK;
		SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
		Resources res = getResources();

		// Each value is a percent from max X/Y stored as an int. Have to bring that value down
		// to a decimal before multiplying by MAX X/Y.
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_A + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_A_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_A + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_A_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_B + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_B_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_B + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_B_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_1 + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_1_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_1 + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_1_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_2 + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_2_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_2 + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_2_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.NUNCHUK_BUTTON_Z + "_X", res.getInteger(R.integer.NUNCHUK_BUTTON_Z_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.NUNCHUK_BUTTON_Z + "_Y", res.getInteger(R.integer.NUNCHUK_BUTTON_Z_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.NUNCHUK_BUTTON_C + "_X", res.getInteger(R.integer.NUNCHUK_BUTTON_C_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.NUNCHUK_BUTTON_C + "_Y", res.getInteger(R.integer.NUNCHUK_BUTTON_C_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_MINUS + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_MINUS + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_PLUS + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_PLUS + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_UP + "_X", res.getInteger(R.integer.WIIMOTE_UP_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_UP + "_Y", res.getInteger(R.integer.WIIMOTE_UP_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_HOME + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_HOME_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_HOME + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_HOME_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.NUNCHUK_STICK + "_X", res.getInteger(R.integer.NUNCHUK_STICK_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.NUNCHUK_STICK + "_Y", res.getInteger(R.integer.NUNCHUK_STICK_Y) / 100.0f);
		// Horizontal dpad
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_RIGHT + "_X", res.getInteger(R.integer.WIIMOTE_RIGHT_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_RIGHT + "_Y", res.getInteger(R.integer.WIIMOTE_RIGHT_Y) / 100.0f);

		// We want to commit right away, otherwise the overlay could load before this is saved.
		sPrefsEditor.commit();
	}

	private void wiiRemoteDefaultOverlay() {
		final int controller = CONTROLLER_WIIREMOTE;
		SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
		Resources res = getResources();

		// Each value is a percent from max X/Y stored as an int. Have to bring that value down
		// to a decimal before multiplying by MAX X/Y.
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_A + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_A_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_A + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_A_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_B + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_B_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_B + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_B_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_1 + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_1_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_1 + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_1_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_2 + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_2_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_2 + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_2_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_MINUS + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_MINUS + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_PLUS + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_PLUS + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_HOME + "_X", res.getInteger(R.integer.WIIMOTE_BUTTON_HOME_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_BUTTON_HOME + "_Y", res.getInteger(R.integer.WIIMOTE_BUTTON_HOME_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_RIGHT + "_X", res.getInteger(R.integer.WIIMOTE_RIGHT_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.WIIMOTE_RIGHT + "_Y", res.getInteger(R.integer.WIIMOTE_RIGHT_Y) / 100.0f);

		// We want to commit right away, otherwise the overlay could load before this is saved.
		sPrefsEditor.commit();
	}

	private void wiiClassicDefaultOverlay() {
		final int controller = COCONTROLLER_CLASSIC;
		SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
		Resources res = getResources();

		// Each value is a percent from max X/Y stored as an int. Have to bring that value down
		// to a decimal before multiplying by MAX X/Y.
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_A + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_A_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_A + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_A_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_B + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_B_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_B + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_B_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_X + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_X_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_X + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_X_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_Y + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_Y_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_Y + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_Y_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_MINUS + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_MINUS_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_MINUS + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_MINUS_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_PLUS + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_PLUS_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_PLUS + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_PLUS_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_HOME + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_HOME_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_HOME + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_HOME_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_ZL + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_ZL_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_ZL + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_ZL_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_ZR + "_X", res.getInteger(R.integer.CLASSIC_BUTTON_ZR_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_BUTTON_ZR + "_Y", res.getInteger(R.integer.CLASSIC_BUTTON_ZR_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_DPAD_UP + "_X", res.getInteger(R.integer.CLASSIC_DPAD_UP_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_DPAD_UP + "_Y", res.getInteger(R.integer.CLASSIC_DPAD_UP_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_STICK_LEFT + "_X", res.getInteger(R.integer.CLASSIC_STICK_LEFT_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_STICK_LEFT + "_Y", res.getInteger(R.integer.CLASSIC_STICK_LEFT_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_STICK_RIGHT + "_X", res.getInteger(R.integer.CLASSIC_STICK_RIGHT_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_STICK_RIGHT + "_Y", res.getInteger(R.integer.CLASSIC_STICK_RIGHT_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_TRIGGER_L + "_X", res.getInteger(R.integer.CLASSIC_TRIGGER_L_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_TRIGGER_L + "_Y", res.getInteger(R.integer.CLASSIC_TRIGGER_L_Y) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_TRIGGER_R + "_X", res.getInteger(R.integer.CLASSIC_TRIGGER_R_X) / 100.0f);
		sPrefsEditor.putFloat(controller + "_" + ButtonType.CLASSIC_TRIGGER_R + "_Y", res.getInteger(R.integer.CLASSIC_TRIGGER_R_Y) / 100.0f);

		// We want to commit right away, otherwise the overlay could load before this is saved.
		sPrefsEditor.commit();
	}
}
