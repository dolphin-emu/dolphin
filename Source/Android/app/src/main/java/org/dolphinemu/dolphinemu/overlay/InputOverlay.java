/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.Display;
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
   * @return The scaled {@link Bitmap}
   */
  public static Bitmap resizeBitmap(Context context, Bitmap bitmap, float scale)
  {
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
  public InputOverlay(Context context, AttributeSet attrs)
  {
    super(context, attrs);

    mPreferences = PreferenceManager.getDefaultSharedPreferences(getContext());
    if (!mPreferences.getBoolean("OverlayInitV2", false))
      defaultOverlay();
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

    for (InputOverlayDrawableJoystick joystick : overlayJoysticks)
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
          if (button.getBounds()
                  .contains((int) event.getX(pointerIndex), (int) event.getY(pointerIndex)))
          {
            button.setPressedState(true);
            button.setTrackId(event.getPointerId(pointerIndex));
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, button.getId(),
                    ButtonState.PRESSED);
          }
          break;
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_POINTER_UP:
          // If a pointer ends, release the button it was pressing.
          if (button.getTrackId() == event.getPointerId(pointerIndex))
          {
            button.setPressedState(false);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, button.getId(),
                    ButtonState.RELEASED);
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
        case MotionEvent.ACTION_MOVE:
          // Up, Down, Left, Right
          boolean[] pressed = {false, false, false, false};
          // If a pointer enters the bounds of a button, press that button.
          if (dpad.getBounds()
                  .contains((int) event.getX(pointerIndex), (int) event.getY(pointerIndex)))
          {
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
                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(i),
                        ButtonState.RELEASED);
            // Press buttons
            for (int i = 0; i < pressed.length; i++)
              if (pressed[i])
                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(i),
                        ButtonState.PRESSED);

            setDpadState(dpad, pressed[0], pressed[1], pressed[2], pressed[3]);
            dpad.setTrackId(event.getPointerId(pointerIndex));
          }
          break;
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_POINTER_UP:
          // If a pointer ends, release the buttons.
          if (dpad.getTrackId() == event.getPointerId(pointerIndex))
          {
            for (int i = 0; i < 4; i++)
            {
              dpad.setState(InputOverlayDrawableDpad.STATE_DEFAULT);
              NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(i),
                      ButtonState.RELEASED);
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
    int fingerPositionX = (int) event.getX(pointerIndex);
    int fingerPositionY = (int) event.getY(pointerIndex);

    String orientation =
            getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT ?
                    "-Portrait" : "";

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
          if (mButtonBeingConfigured == null &&
                  button.getBounds().contains(fingerPositionX, fingerPositionY))
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
            saveControlPosition(mButtonBeingConfigured.getId(),
                    mButtonBeingConfigured.getBounds().left,
                    mButtonBeingConfigured.getBounds().top, orientation);
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
          if (mButtonBeingConfigured == null &&
                  dpad.getBounds().contains(fingerPositionX, fingerPositionY))
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
            saveControlPosition(mDpadBeingConfigured.getId(0),
                    mDpadBeingConfigured.getBounds().left, mDpadBeingConfigured.getBounds().top,
                    orientation);
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
          if (mJoystickBeingConfigured == null &&
                  joystick.getBounds().contains(fingerPositionX, fingerPositionY))
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
            saveControlPosition(mJoystickBeingConfigured.getId(),
                    mJoystickBeingConfigured.getBounds().left,
                    mJoystickBeingConfigured.getBounds().top, orientation);
            mJoystickBeingConfigured = null;
          }
          break;
      }
    }

    return true;
  }

  private void setDpadState(InputOverlayDrawableDpad dpad, boolean up, boolean down, boolean left,
          boolean right)
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

  private void addGameCubeOverlayControls(String orientation)
  {
    if (mPreferences.getBoolean("buttonToggleGc0", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_a,
              R.drawable.gcpad_a_pressed, ButtonType.BUTTON_A, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc1", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_b,
              R.drawable.gcpad_b_pressed, ButtonType.BUTTON_B, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc2", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_x,
              R.drawable.gcpad_x_pressed, ButtonType.BUTTON_X, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc3", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_y,
              R.drawable.gcpad_y_pressed, ButtonType.BUTTON_Y, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc4", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_z,
              R.drawable.gcpad_z_pressed, ButtonType.BUTTON_Z, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc5", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_start,
              R.drawable.gcpad_start_pressed, ButtonType.BUTTON_START, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc6", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_l,
              R.drawable.gcpad_l_pressed, ButtonType.TRIGGER_L, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc7", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.gcpad_r,
              R.drawable.gcpad_r_pressed, ButtonType.TRIGGER_R, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc8", true))
    {
      overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.gcwii_dpad,
              R.drawable.gcwii_dpad_pressed_one_direction,
              R.drawable.gcwii_dpad_pressed_two_directions,
              ButtonType.BUTTON_UP, ButtonType.BUTTON_DOWN,
              ButtonType.BUTTON_LEFT, ButtonType.BUTTON_RIGHT, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc9", true))
    {
      overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
              R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed, ButtonType.STICK_MAIN,
              orientation));
    }
    if (mPreferences.getBoolean("buttonToggleGc10", true))
    {
      overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
              R.drawable.gcpad_c, R.drawable.gcpad_c_pressed, ButtonType.STICK_C, orientation));
    }
  }

  private void addWiimoteOverlayControls(String orientation)
  {
    if (mPreferences.getBoolean("buttonToggleWii0", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_a,
              R.drawable.wiimote_a_pressed, ButtonType.WIIMOTE_BUTTON_A, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii1", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_b,
              R.drawable.wiimote_b_pressed, ButtonType.WIIMOTE_BUTTON_B, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii2", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_one,
              R.drawable.wiimote_one_pressed, ButtonType.WIIMOTE_BUTTON_1, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii3", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_two,
              R.drawable.wiimote_two_pressed, ButtonType.WIIMOTE_BUTTON_2, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii4", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_plus,
              R.drawable.wiimote_plus_pressed, ButtonType.WIIMOTE_BUTTON_PLUS, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii5", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_minus,
              R.drawable.wiimote_minus_pressed, ButtonType.WIIMOTE_BUTTON_MINUS, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii6", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_home,
              R.drawable.wiimote_home_pressed, ButtonType.WIIMOTE_BUTTON_HOME, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii7", true))
    {
      if (mPreferences.getInt("wiiController", 3) == 2)
      {
        overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.gcwii_dpad,
                R.drawable.gcwii_dpad_pressed_one_direction,
                R.drawable.gcwii_dpad_pressed_two_directions,
                ButtonType.WIIMOTE_RIGHT, ButtonType.WIIMOTE_LEFT,
                ButtonType.WIIMOTE_UP, ButtonType.WIIMOTE_DOWN, orientation));
      }
      else
      {
        overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.gcwii_dpad,
                R.drawable.gcwii_dpad_pressed_one_direction,
                R.drawable.gcwii_dpad_pressed_two_directions,
                ButtonType.WIIMOTE_UP, ButtonType.WIIMOTE_DOWN,
                ButtonType.WIIMOTE_LEFT, ButtonType.WIIMOTE_RIGHT, orientation));
      }
    }
  }

  private void addNunchukOverlayControls(String orientation)
  {
    if (mPreferences.getBoolean("buttonToggleWii8", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.nunchuk_c,
              R.drawable.nunchuk_c_pressed, ButtonType.NUNCHUK_BUTTON_C, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii9", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.nunchuk_z,
              R.drawable.nunchuk_z_pressed, ButtonType.NUNCHUK_BUTTON_Z, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleWii10", true))
    {
      overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
              R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed,
              ButtonType.NUNCHUK_STICK, orientation));
    }
  }

  private void addClassicOverlayControls(String orientation)
  {
    if (mPreferences.getBoolean("buttonToggleClassic0", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_a,
              R.drawable.classic_a_pressed, ButtonType.CLASSIC_BUTTON_A, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic1", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_b,
              R.drawable.classic_b_pressed, ButtonType.CLASSIC_BUTTON_B, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic2", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_x,
              R.drawable.classic_x_pressed, ButtonType.CLASSIC_BUTTON_X, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic3", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_y,
              R.drawable.classic_y_pressed, ButtonType.CLASSIC_BUTTON_Y, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic4", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_plus,
              R.drawable.wiimote_plus_pressed, ButtonType.CLASSIC_BUTTON_PLUS, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic5", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_minus,
              R.drawable.wiimote_minus_pressed, ButtonType.CLASSIC_BUTTON_MINUS, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic6", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.wiimote_home,
              R.drawable.wiimote_home_pressed, ButtonType.CLASSIC_BUTTON_HOME, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic7", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_l,
              R.drawable.classic_l_pressed, ButtonType.CLASSIC_TRIGGER_L, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic8", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_r,
              R.drawable.classic_r_pressed, ButtonType.CLASSIC_TRIGGER_R, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic9", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_zl,
              R.drawable.classic_zl_pressed, ButtonType.CLASSIC_BUTTON_ZL, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic10", true))
    {
      overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.classic_zr,
              R.drawable.classic_zr_pressed, ButtonType.CLASSIC_BUTTON_ZR, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic11", true))
    {
      overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.gcwii_dpad,
              R.drawable.gcwii_dpad_pressed_one_direction,
              R.drawable.gcwii_dpad_pressed_two_directions,
              ButtonType.CLASSIC_DPAD_UP, ButtonType.CLASSIC_DPAD_DOWN,
              ButtonType.CLASSIC_DPAD_LEFT, ButtonType.CLASSIC_DPAD_RIGHT, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic12", true))
    {
      overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
              R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed,
              ButtonType.CLASSIC_STICK_LEFT, orientation));
    }
    if (mPreferences.getBoolean("buttonToggleClassic13", true))
    {
      overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.gcwii_joystick_range,
              R.drawable.gcwii_joystick, R.drawable.gcwii_joystick_pressed,
              ButtonType.CLASSIC_STICK_RIGHT, orientation));
    }
  }

  public void refreshControls()
  {
    // Remove all the overlay buttons from the HashSet.
    overlayButtons.removeAll(overlayButtons);
    overlayDpads.removeAll(overlayDpads);
    overlayJoysticks.removeAll(overlayJoysticks);

    String orientation =
            getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT ?
                    "-Portrait" : "";

    // Add all the enabled overlay items back to the HashSet.
    if (EmulationActivity.isGameCubeGame() || mPreferences.getInt("wiiController", 3) == 0)
    {
      addGameCubeOverlayControls(orientation);
    }
    else if (mPreferences.getInt("wiiController", 3) == 4)
    {
      addClassicOverlayControls(orientation);
    }
    else
    {
      addWiimoteOverlayControls(orientation);
      if (mPreferences.getInt("wiiController", 3) == 3)
      {
        addNunchukOverlayControls(orientation);
      }
    }

    invalidate();
  }

  public void resetButtonPlacement()
  {
    boolean isLandscape =
            getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE;

    // Values for these come from R.array.controllersEntries
    if (EmulationActivity.isGameCubeGame() || mPreferences.getInt("wiiController", 3) == 0)
    {
      if (isLandscape)
        gcDefaultOverlay();
      else
        gcPortraitDefaultOverlay();
    }
    else if (mPreferences.getInt("wiiController", 3) == 4)
    {
      if (isLandscape)
        wiiClassicDefaultOverlay();
      else
        wiiClassicPortraitDefaultOverlay();
    }
    else
    {
      if (isLandscape)
        wiiDefaultOverlay();
      else
        wiiPortraitDefaultOverlay();
    }
    refreshControls();
  }

  private void saveControlPosition(int sharedPrefsId, int x, int y, String orientation)
  {
    final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(getContext());
    SharedPreferences.Editor sPrefsEditor = sPrefs.edit();
    sPrefsEditor.putFloat(sharedPrefsId + orientation + "-X", x);
    sPrefsEditor.putFloat(sharedPrefsId + orientation + "-Y", y);
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
   * @param context      The current {@link Context}.
   * @param defaultResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Default State).
   * @param pressedResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Pressed State).
   * @param buttonId     Identifier for determining what type of button the initialized InputOverlayDrawableButton represents.
   * @return An {@link InputOverlayDrawableButton} with the correct drawing bounds set.
   */
  private static InputOverlayDrawableButton initializeOverlayButton(Context context,
          int defaultResId, int pressedResId, int buttonId, String orientation)
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
    final Bitmap defaultStateBitmap =
            resizeBitmap(context, BitmapFactory.decodeResource(res, defaultResId), scale);
    final Bitmap pressedStateBitmap =
            resizeBitmap(context, BitmapFactory.decodeResource(res, pressedResId), scale);
    final InputOverlayDrawableButton overlayDrawable =
            new InputOverlayDrawableButton(res, defaultStateBitmap, pressedStateBitmap, buttonId);

    // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
    // These were set in the input overlay configuration menu.
    int drawableX = (int) sPrefs.getFloat(buttonId + orientation + "-X", 0f);
    int drawableY = (int) sPrefs.getFloat(buttonId + orientation + "-Y", 0f);

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
   * @return the initialized {@link InputOverlayDrawableDpad}
   */
  private static InputOverlayDrawableDpad initializeOverlayDpad(Context context,
          int defaultResId,
          int pressedOneDirectionResId,
          int pressedTwoDirectionsResId,
          int buttonUp,
          int buttonDown,
          int buttonLeft,
          int buttonRight,
          String orientation)
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
    final Bitmap defaultStateBitmap =
            resizeBitmap(context, BitmapFactory.decodeResource(res, defaultResId), scale);
    final Bitmap pressedOneDirectionStateBitmap =
            resizeBitmap(context, BitmapFactory.decodeResource(res, pressedOneDirectionResId),
                    scale);
    final Bitmap pressedTwoDirectionsStateBitmap =
            resizeBitmap(context, BitmapFactory.decodeResource(res, pressedTwoDirectionsResId),
                    scale);
    final InputOverlayDrawableDpad overlayDrawable =
            new InputOverlayDrawableDpad(res, defaultStateBitmap,
                    pressedOneDirectionStateBitmap, pressedTwoDirectionsStateBitmap,
                    buttonUp, buttonDown, buttonLeft, buttonRight);

    // The X and Y coordinates of the InputOverlayDrawableDpad on the InputOverlay.
    // These were set in the input overlay configuration menu.
    int drawableX = (int) sPrefs.getFloat(buttonUp + orientation + "-X", 0f);
    int drawableY = (int) sPrefs.getFloat(buttonUp + orientation + "-Y", 0f);

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
   * @return the initialized {@link InputOverlayDrawableJoystick}.
   */
  private static InputOverlayDrawableJoystick initializeOverlayJoystick(Context context,
          int resOuter, int defaultResInner, int pressedResInner, int joystick, String orientation)
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
    final Bitmap bitmapOuter =
            resizeBitmap(context, BitmapFactory.decodeResource(res, resOuter), scale);
    final Bitmap bitmapInnerDefault = BitmapFactory.decodeResource(res, defaultResInner);
    final Bitmap bitmapInnerPressed = BitmapFactory.decodeResource(res, pressedResInner);

    // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
    // These were set in the input overlay configuration menu.
    int drawableX = (int) sPrefs.getFloat(joystick + orientation + "-X", 0f);
    int drawableY = (int) sPrefs.getFloat(joystick + orientation + "-Y", 0f);

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
            outerRect, innerRect, joystick, sPrefs);

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

  private void defaultOverlay()
  {
    // It's possible that a user has created their overlay before this was added
    // Only change the overlay if the 'A' button is not in the upper corner.
    // GameCube
    if (mPreferences.getFloat(ButtonType.BUTTON_A + "-X", 0f) == 0f)
    {
      gcDefaultOverlay();
    }
    if (mPreferences.getFloat(ButtonType.BUTTON_A + "-Portrait" + "-X", 0f) == 0f)
    {
      gcPortraitDefaultOverlay();
    }

    // Wii
    if (mPreferences.getFloat(ButtonType.WIIMOTE_BUTTON_A + "-X", 0f) == 0f)
    {
      wiiDefaultOverlay();
    }
    if (mPreferences.getFloat(ButtonType.WIIMOTE_BUTTON_A + "-Portrait" + "-X", 0f) == 0f)
    {
      wiiPortraitDefaultOverlay();
    }

    // Wii Classic
    if (mPreferences.getFloat(ButtonType.CLASSIC_BUTTON_A + "-X", 0f) == 0f)
    {
      wiiClassicDefaultOverlay();
    }
    if (mPreferences.getFloat(ButtonType.CLASSIC_BUTTON_A + "-Portrait" + "-X", 0f) == 0f)
    {
      wiiClassicPortraitDefaultOverlay();
    }

    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
    sPrefsEditor.putBoolean("OverlayInitV2", true);
    sPrefsEditor.apply();
  }

  private void gcDefaultOverlay()
  {
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();

    // Get screen size
    Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
    DisplayMetrics outMetrics = new DisplayMetrics();
    display.getMetrics(outMetrics);
    float maxX = outMetrics.heightPixels;
    float maxY = outMetrics.widthPixels;
    // Height and width changes depending on orientation. Use the larger value for height.
    if (maxY > maxX)
    {
      float tmp = maxX;
      maxX = maxY;
      maxY = tmp;
    }
    Resources res = getResources();

    // Each value is a percent from max X/Y stored as an int. Have to bring that value down
    // to a decimal before multiplying by MAX X/Y.
    sPrefsEditor.putFloat(ButtonType.BUTTON_A + "-X",
            (((float) res.getInteger(R.integer.BUTTON_A_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_A + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_A_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_B + "-X",
            (((float) res.getInteger(R.integer.BUTTON_B_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_B + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_B_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_X + "-X",
            (((float) res.getInteger(R.integer.BUTTON_X_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_X + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_X_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_Y + "-X",
            (((float) res.getInteger(R.integer.BUTTON_Y_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_Y + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_Y_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_Z + "-X",
            (((float) res.getInteger(R.integer.BUTTON_Z_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_Z + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_Z_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_UP + "-X",
            (((float) res.getInteger(R.integer.BUTTON_UP_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_UP + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_UP_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.TRIGGER_L + "-X",
            (((float) res.getInteger(R.integer.TRIGGER_L_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.TRIGGER_L + "-Y",
            (((float) res.getInteger(R.integer.TRIGGER_L_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.TRIGGER_R + "-X",
            (((float) res.getInteger(R.integer.TRIGGER_R_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.TRIGGER_R + "-Y",
            (((float) res.getInteger(R.integer.TRIGGER_R_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_START + "-X",
            (((float) res.getInteger(R.integer.BUTTON_START_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_START + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_START_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.STICK_C + "-X",
            (((float) res.getInteger(R.integer.STICK_C_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.STICK_C + "-Y",
            (((float) res.getInteger(R.integer.STICK_C_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.STICK_MAIN + "-X",
            (((float) res.getInteger(R.integer.STICK_MAIN_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.STICK_MAIN + "-Y",
            (((float) res.getInteger(R.integer.STICK_MAIN_Y) / 1000) * maxY));

    // We want to commit right away, otherwise the overlay could load before this is saved.
    sPrefsEditor.commit();
  }


  private void gcPortraitDefaultOverlay()
  {
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();

    // Get screen size
    Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
    DisplayMetrics outMetrics = new DisplayMetrics();
    display.getMetrics(outMetrics);
    float maxX = outMetrics.heightPixels;
    float maxY = outMetrics.widthPixels;
    // Height and width changes depending on orientation. Use the larger value for height.
    if (maxY < maxX)
    {
      float tmp = maxX;
      maxX = maxY;
      maxY = tmp;
    }
    Resources res = getResources();
    String portrait = "-Portrait";

    // Each value is a percent from max X/Y stored as an int. Have to bring that value down
    // to a decimal before multiplying by MAX X/Y.
    sPrefsEditor.putFloat(ButtonType.BUTTON_A + portrait + "-X",
            (((float) res.getInteger(R.integer.BUTTON_A_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_A + portrait + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_A_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_B + portrait + "-X",
            (((float) res.getInteger(R.integer.BUTTON_B_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_B + portrait + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_B_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_X + portrait + "-X",
            (((float) res.getInteger(R.integer.BUTTON_X_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_X + portrait + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_X_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_Y + portrait + "-X",
            (((float) res.getInteger(R.integer.BUTTON_Y_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_Y + portrait + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_Y_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_Z + portrait + "-X",
            (((float) res.getInteger(R.integer.BUTTON_Z_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_Z + portrait + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_Z_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_UP + portrait + "-X",
            (((float) res.getInteger(R.integer.BUTTON_UP_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_UP + portrait + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_UP_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.TRIGGER_L + portrait + "-X",
            (((float) res.getInteger(R.integer.TRIGGER_L_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.TRIGGER_L + portrait + "-Y",
            (((float) res.getInteger(R.integer.TRIGGER_L_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.TRIGGER_R + portrait + "-X",
            (((float) res.getInteger(R.integer.TRIGGER_R_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.TRIGGER_R + portrait + "-Y",
            (((float) res.getInteger(R.integer.TRIGGER_R_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.BUTTON_START + portrait + "-X",
            (((float) res.getInteger(R.integer.BUTTON_START_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.BUTTON_START + portrait + "-Y",
            (((float) res.getInteger(R.integer.BUTTON_START_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.STICK_C + portrait + "-X",
            (((float) res.getInteger(R.integer.STICK_C_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.STICK_C + portrait + "-Y",
            (((float) res.getInteger(R.integer.STICK_C_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.STICK_MAIN + portrait + "-X",
            (((float) res.getInteger(R.integer.STICK_MAIN_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.STICK_MAIN + portrait + "-Y",
            (((float) res.getInteger(R.integer.STICK_MAIN_PORTRAIT_Y) / 1000) * maxY));

    // We want to commit right away, otherwise the overlay could load before this is saved.
    sPrefsEditor.commit();
  }

  private void wiiDefaultOverlay()
  {
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();

    // Get screen size
    Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
    DisplayMetrics outMetrics = new DisplayMetrics();
    display.getMetrics(outMetrics);
    float maxX = outMetrics.heightPixels;
    float maxY = outMetrics.widthPixels;
    // Height and width changes depending on orientation. Use the larger value for maxX.
    if (maxY > maxX)
    {
      float tmp = maxX;
      maxX = maxY;
      maxY = tmp;
    }
    Resources res = getResources();

    // Each value is a percent from max X/Y stored as an int. Have to bring that value down
    // to a decimal before multiplying by MAX X/Y.
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_A + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_A_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_A + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_A_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_B + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_B_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_B + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_B_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_1 + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_1_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_1 + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_1_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_2 + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_2_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_2 + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_2_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_BUTTON_Z + "-X",
            (((float) res.getInteger(R.integer.NUNCHUK_BUTTON_Z_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_BUTTON_Z + "-Y",
            (((float) res.getInteger(R.integer.NUNCHUK_BUTTON_Z_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_BUTTON_C + "-X",
            (((float) res.getInteger(R.integer.NUNCHUK_BUTTON_C_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_BUTTON_C + "-Y",
            (((float) res.getInteger(R.integer.NUNCHUK_BUTTON_C_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_MINUS + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_MINUS + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_PLUS + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_PLUS + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_UP + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_UP_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_UP + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_UP_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_HOME + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_HOME_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_HOME + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_HOME_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_STICK + "-X",
            (((float) res.getInteger(R.integer.NUNCHUK_STICK_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_STICK + "-Y",
            (((float) res.getInteger(R.integer.NUNCHUK_STICK_Y) / 1000) * maxY));
    // Horizontal dpad
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_RIGHT + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_RIGHT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_RIGHT + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_RIGHT_Y) / 1000) * maxY));

    // We want to commit right away, otherwise the overlay could load before this is saved.
    sPrefsEditor.commit();
  }

  private void wiiPortraitDefaultOverlay()
  {
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();

    // Get screen size
    Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
    DisplayMetrics outMetrics = new DisplayMetrics();
    display.getMetrics(outMetrics);
    float maxX = outMetrics.heightPixels;
    float maxY = outMetrics.widthPixels;
    // Height and width changes depending on orientation. Use the larger value for maxX.
    if (maxY < maxX)
    {
      float tmp = maxX;
      maxX = maxY;
      maxY = tmp;
    }
    Resources res = getResources();
    String portrait = "-Portrait";

    // Each value is a percent from max X/Y stored as an int. Have to bring that value down
    // to a decimal before multiplying by MAX X/Y.
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_A + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_A_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_A + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_A_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_B + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_B_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_B + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_B_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_1 + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_1_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_1 + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_1_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_2 + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_2_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_2 + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_2_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_BUTTON_Z + portrait + "-X",
            (((float) res.getInteger(R.integer.NUNCHUK_BUTTON_Z_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_BUTTON_Z + portrait + "-Y",
            (((float) res.getInteger(R.integer.NUNCHUK_BUTTON_Z_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_BUTTON_C + portrait + "-X",
            (((float) res.getInteger(R.integer.NUNCHUK_BUTTON_C_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_BUTTON_C + portrait + "-Y",
            (((float) res.getInteger(R.integer.NUNCHUK_BUTTON_C_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_MINUS + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_MINUS + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_PLUS + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_PLUS + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_UP + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_UP_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_UP + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_UP_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_HOME + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_HOME_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_BUTTON_HOME + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_BUTTON_HOME_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_STICK + portrait + "-X",
            (((float) res.getInteger(R.integer.NUNCHUK_STICK_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.NUNCHUK_STICK + portrait + "-Y",
            (((float) res.getInteger(R.integer.NUNCHUK_STICK_PORTRAIT_Y) / 1000) * maxY));
    // Horizontal dpad
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_RIGHT + portrait + "-X",
            (((float) res.getInteger(R.integer.WIIMOTE_RIGHT_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.WIIMOTE_RIGHT + portrait + "-Y",
            (((float) res.getInteger(R.integer.WIIMOTE_RIGHT_PORTRAIT_Y) / 1000) * maxY));

    // We want to commit right away, otherwise the overlay could load before this is saved.
    sPrefsEditor.commit();
  }

  private void wiiClassicDefaultOverlay()
  {
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();

    // Get screen size
    Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
    DisplayMetrics outMetrics = new DisplayMetrics();
    display.getMetrics(outMetrics);
    float maxX = outMetrics.heightPixels;
    float maxY = outMetrics.widthPixels;
    // Height and width changes depending on orientation. Use the larger value for maxX.
    if (maxY > maxX)
    {
      float tmp = maxX;
      maxX = maxY;
      maxY = tmp;
    }
    Resources res = getResources();

    // Each value is a percent from max X/Y stored as an int. Have to bring that value down
    // to a decimal before multiplying by MAX X/Y.
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_A + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_A_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_A + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_A_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_B + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_B_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_B + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_B_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_X + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_X_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_X + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_X_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_Y + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_Y_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_Y + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_Y_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_MINUS + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_MINUS_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_MINUS + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_MINUS_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_PLUS + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_PLUS_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_PLUS + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_PLUS_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_HOME + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_HOME_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_HOME + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_HOME_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_ZL + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_ZL_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_ZL + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_ZL_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_ZR + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_ZR_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_ZR + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_ZR_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_DPAD_UP + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_DPAD_UP_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_DPAD_UP + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_DPAD_UP_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_STICK_LEFT + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_STICK_LEFT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_STICK_LEFT + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_STICK_LEFT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_STICK_RIGHT + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_STICK_RIGHT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_STICK_RIGHT + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_STICK_RIGHT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_TRIGGER_L + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_TRIGGER_L_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_TRIGGER_L + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_TRIGGER_L_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_TRIGGER_R + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_TRIGGER_R_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_TRIGGER_R + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_TRIGGER_R_Y) / 1000) * maxY));

    // We want to commit right away, otherwise the overlay could load before this is saved.
    sPrefsEditor.commit();
  }


  private void wiiClassicPortraitDefaultOverlay()
  {
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();

    // Get screen size
    Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
    DisplayMetrics outMetrics = new DisplayMetrics();
    display.getMetrics(outMetrics);
    float maxX = outMetrics.heightPixels;
    float maxY = outMetrics.widthPixels;
    // Height and width changes depending on orientation. Use the larger value for maxX.
    if (maxY < maxX)
    {
      float tmp = maxX;
      maxX = maxY;
      maxY = tmp;
    }
    Resources res = getResources();
    String portrait = "-Portrait";

    // Each value is a percent from max X/Y stored as an int. Have to bring that value down
    // to a decimal before multiplying by MAX X/Y.
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_A + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_A_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_A + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_A_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_B + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_B_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_B + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_B_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_X + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_X_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_X + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_X_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_Y + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_Y_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_Y + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_Y_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_MINUS + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_MINUS_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_MINUS + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_MINUS_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_PLUS + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_PLUS_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_PLUS + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_PLUS_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_HOME + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_HOME_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_HOME + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_HOME_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_ZL + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_ZL_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_ZL + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_ZL_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_ZR + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_ZR_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_BUTTON_ZR + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_BUTTON_ZR_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_DPAD_UP + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_DPAD_UP_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_DPAD_UP + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_DPAD_UP_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_STICK_LEFT + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_STICK_LEFT_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_STICK_LEFT + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_STICK_LEFT_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_STICK_RIGHT + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_STICK_RIGHT_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_STICK_RIGHT + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_STICK_RIGHT_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_TRIGGER_L + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_TRIGGER_L_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_TRIGGER_L + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_TRIGGER_L_PORTRAIT_Y) / 1000) * maxY));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_TRIGGER_R + portrait + "-X",
            (((float) res.getInteger(R.integer.CLASSIC_TRIGGER_R_PORTRAIT_X) / 1000) * maxX));
    sPrefsEditor.putFloat(ButtonType.CLASSIC_TRIGGER_R + portrait + "-Y",
            (((float) res.getInteger(R.integer.CLASSIC_TRIGGER_R_PORTRAIT_Y) / 1000) * maxY));

    // We want to commit right away, otherwise the overlay could load before this is saved.
    sPrefsEditor.commit();
  }
}
