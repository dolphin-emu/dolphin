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
import android.graphics.drawable.BitmapDrawable;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnTouchListener;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonType;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

import java.util.ArrayList;

/**
 * Draws the interactive input overlay on top of the
 * {@link SurfaceView} that is rendering emulation.
 */
public final class InputOverlay extends SurfaceView implements OnTouchListener
{
  public static final String CONTROL_INIT_PREF_KEY = "InitOverlay";
  public static final String CONTROL_SCALE_PREF_KEY = "ControlScale";
  public static int sControllerScale;
  public static final String CONTROL_ALPHA_PREF_KEY = "ControlAlpha";
  public static int sControllerAlpha;

  public static final String POINTER_PREF_KEY = "TouchPointer1";
  public static final String RECENTER_PREF_KEY = "IRRecenter";
  public static boolean sIRRecenter;
  public static final String RELATIVE_PREF_KEY = "JoystickRelative";
  public static boolean sJoystickRelative;

  public static final String CONTROL_TYPE_PREF_KEY = "WiiController";
  public static final int CONTROLLER_GAMECUBE = 0;
  public static final int CONTROLLER_CLASSIC = 1;
  public static final int CONTROLLER_WIINUNCHUK = 2;
  public static final int CONTROLLER_WIIREMOTE = 3;
  public static int sControllerType;

  public static final String JOYSTICK_PREF_KEY = "JoystickEmulate";
  public static final int JOYSTICK_EMULATE_NONE = 0;
  public static final int JOYSTICK_EMULATE_IR = 1;
  public static final int JOYSTICK_EMULATE_WII_SWING = 2;
  public static final int JOYSTICK_EMULATE_WII_TILT = 3;
  public static final int JOYSTICK_EMULATE_WII_SHAKE = 4;
  public static final int JOYSTICK_EMULATE_NUNCHUK_SWING = 5;
  public static final int JOYSTICK_EMULATE_NUNCHUK_TILT = 6;
  public static final int JOYSTICK_EMULATE_NUNCHUK_SHAKE = 7;
  public static int sJoyStickSetting;

  public static final int SENSOR_GC_NONE = 0;
  public static final int SENSOR_GC_JOYSTICK = 1;
  public static final int SENSOR_GC_CSTICK = 2;
  public static final int SENSOR_GC_DPAD = 3;
  public static int sSensorGCSetting;

  public static final int SENSOR_WII_NONE = 0;
  public static final int SENSOR_WII_DPAD = 1;
  public static final int SENSOR_WII_STICK = 2;
  public static final int SENSOR_WII_IR = 3;
  public static final int SENSOR_WII_SWING = 4;
  public static final int SENSOR_WII_TILT = 5;
  public static final int SENSOR_WII_SHAKE = 6;
  public static final int SENSOR_NUNCHUK_SWING = 7;
  public static final int SENSOR_NUNCHUK_TILT = 8;
  public static final int SENSOR_NUNCHUK_SHAKE = 9;
  public static int sSensorWiiSetting;

  public static int[] sShakeStates = new int[4];

  // input hack for RK4JAF
  public static int sInputHackForRK4 = -1;

  private final ArrayList<InputOverlayDrawableButton> mButtons = new ArrayList<>();
  private final ArrayList<InputOverlayDrawableDpad> mDpads = new ArrayList<>();
  private final ArrayList<InputOverlayDrawableJoystick> mJoysticks = new ArrayList<>();
  private InputOverlayPointer mOverlayPointer = null;
  private InputOverlaySensor mOverlaySensor = null;

  private boolean mIsInEditMode = false;
  private InputOverlayDrawableButton mButtonBeingConfigured;
  private InputOverlayDrawableDpad mDpadBeingConfigured;
  private InputOverlayDrawableJoystick mJoystickBeingConfigured;

  private SharedPreferences mPreferences;

  /**
   * Constructor
   *
   * @param context The current {@link Context}.
   * @param attrs   {@link AttributeSet} for parsing XML attributes.
   */
  public InputOverlay(Context context, AttributeSet attrs)
  {
    super(context, attrs);

    // input hack
    String gameId = EmulationActivity.get().getSelectedGameId();
    if (gameId != null && gameId.length() > 3 && gameId.substring(0, 3).equals("RK4"))
    {
      sInputHackForRK4 = NativeLibrary.ButtonType.WIIMOTE_BUTTON_A;
    }

    mPreferences = PreferenceManager.getDefaultSharedPreferences(context);
    if (!mPreferences.getBoolean(CONTROL_INIT_PREF_KEY, false))
      defaultOverlay();

    // initialize shake states
    for(int i = 0; i < sShakeStates.length; ++i)
    {
      sShakeStates[i] = NativeLibrary.ButtonState.RELEASED;
    }

    // init touch pointer
    int touchPointer = 0;
    if(!EmulationActivity.get().isGameCubeGame())
      touchPointer = mPreferences.getInt(InputOverlay.POINTER_PREF_KEY, 0);
    setTouchPointer(touchPointer);

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

    for (InputOverlayDrawableButton button : mButtons)
    {
      button.onDraw(canvas);
    }

    for (InputOverlayDrawableDpad dpad : mDpads)
    {
      dpad.onDraw(canvas);
    }

    for (InputOverlayDrawableJoystick joystick : mJoysticks)
    {
      joystick.onDraw(canvas);
    }
  }

  @Override
  public boolean onTouch(View v, MotionEvent event)
  {
    if (isInEditMode())
    {
      return onTouchWhileEditing(event);
    }

    boolean isProcessed = false;
    switch (event.getAction() & MotionEvent.ACTION_MASK)
    {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
      {
        int pointerIndex = event.getActionIndex();
        int pointerId = event.getPointerId(pointerIndex);
        float pointerX = event.getX(pointerIndex);
        float pointerY = event.getY(pointerIndex);

        for (InputOverlayDrawableJoystick joystick : mJoysticks)
        {
          if(joystick.getBounds().contains((int)pointerX, (int)pointerY))
          {
            joystick.onPointerDown(pointerId, pointerX, pointerY);
            isProcessed = true;
            break;
          }
        }

        for (InputOverlayDrawableButton button : mButtons)
        {
          if (button.getBounds().contains((int)pointerX, (int)pointerY))
          {
            button.onPointerDown(pointerId, pointerX, pointerY);
            isProcessed = true;
          }
        }

        for (InputOverlayDrawableDpad dpad : mDpads)
        {
          if (dpad.getBounds().contains((int)pointerX, (int)pointerY))
          {
            dpad.onPointerDown(pointerId, pointerX, pointerY);
            isProcessed = true;
          }
        }

        if(!isProcessed && mOverlayPointer != null && mOverlayPointer.getPointerId() == -1)
          mOverlayPointer.onPointerDown(pointerId, pointerX, pointerY);
        break;
      }
      case MotionEvent.ACTION_MOVE:
      {
        int pointerCount = event.getPointerCount();
        for (int i = 0; i < pointerCount; ++i)
        {
          boolean isCaptured = false;
          int pointerId = event.getPointerId(i);
          float pointerX = event.getX(i);
          float pointerY = event.getY(i);

          for (InputOverlayDrawableJoystick joystick : mJoysticks)
          {
            if(joystick.getPointerId() == pointerId)
            {
              joystick.onPointerMove(pointerId, pointerX, pointerY);
              isCaptured = true;
              isProcessed = true;
              break;
            }
          }
          if(isCaptured)
            continue;

          for (InputOverlayDrawableButton button : mButtons)
          {
            if (button.getBounds().contains((int)pointerX, (int)pointerY))
            {
              if(button.getPointerId() == -1)
              {
                button.onPointerDown(pointerId, pointerX, pointerY);
                isProcessed = true;
              }
            }
            else if(button.getPointerId() == pointerId)
            {
              button.onPointerUp(pointerId, pointerX, pointerY);
              isProcessed = true;
            }
          }

          for (InputOverlayDrawableDpad dpad : mDpads)
          {
            if(dpad.getPointerId() == pointerId)
            {
              dpad.onPointerMove(pointerId, pointerX, pointerY);
              isProcessed = true;
            }
          }

          if(mOverlayPointer != null && mOverlayPointer.getPointerId() == pointerId)
          {
            mOverlayPointer.onPointerMove(pointerId, pointerX, pointerY);
          }
        }
        break;
      }

      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_POINTER_UP:
      {
        int pointerIndex = event.getActionIndex();
        int pointerId = event.getPointerId(pointerIndex);
        float pointerX = event.getX(pointerIndex);
        float pointerY = event.getY(pointerIndex);

        if(mOverlayPointer != null && mOverlayPointer.getPointerId() == pointerId)
        {
          mOverlayPointer.onPointerUp(pointerId, pointerX, pointerY);
        }

        for (InputOverlayDrawableJoystick joystick : mJoysticks)
        {
          if(joystick.getPointerId() == pointerId)
          {
            joystick.onPointerUp(pointerId, pointerX, pointerY);
            isProcessed = true;
            break;
          }
        }

        for (InputOverlayDrawableButton button : mButtons)
        {
          if(button.getPointerId() == pointerId)
          {
            button.onPointerUp(pointerId, pointerX, pointerY);
            if (mOverlayPointer != null && button.getButtonId() == ButtonType.HOTKEYS_UPRIGHT_TOGGLE)
            {
              mOverlayPointer.reset();
            }
            isProcessed = true;
          }
        }

        for (InputOverlayDrawableDpad dpad : mDpads)
        {
          if (dpad.getPointerId() == pointerId)
          {
            dpad.onPointerUp(pointerId, pointerX, pointerY);
            isProcessed = true;
          }
        }
        break;
      }

      case MotionEvent.ACTION_CANCEL:
      {
        isProcessed = true;
        if(mOverlayPointer != null)
        {
          mOverlayPointer.onPointerUp(0, 0, 0);
        }

        for (InputOverlayDrawableJoystick joystick : mJoysticks)
        {
          joystick.onPointerUp(0, 0, 0);
        }

        for (InputOverlayDrawableButton button : mButtons)
        {
          button.onPointerUp(0, 0, 0);
        }

        for (InputOverlayDrawableDpad dpad : mDpads)
        {
          dpad.onPointerUp(0, 0, 0);
        }
        break;
      }
    }

    if(isProcessed)
      invalidate();

    return true;
  }

  public boolean onTouchWhileEditing(MotionEvent event)
  {
    int pointerIndex = event.getActionIndex();
    int pointerX = (int) event.getX(pointerIndex);
    int pointerY = (int) event.getY(pointerIndex);

    switch (event.getAction() & MotionEvent.ACTION_MASK)
    {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
        if (mButtonBeingConfigured != null || mDpadBeingConfigured != null || mJoystickBeingConfigured != null)
          return false;
        for (InputOverlayDrawableButton button : mButtons)
        {
          if (button.getBounds().contains(pointerX, pointerY))
          {
            mButtonBeingConfigured = button;
            mButtonBeingConfigured.onConfigureBegin(pointerX, pointerY);
            return true;
          }
        }
        for (InputOverlayDrawableDpad dpad : mDpads)
        {
          if (dpad.getBounds().contains(pointerX, pointerY))
          {
            mDpadBeingConfigured = dpad;
            mDpadBeingConfigured.onConfigureBegin(pointerX, pointerY);
            return true;
          }
        }
        for (InputOverlayDrawableJoystick joystick : mJoysticks)
        {
          if (joystick.getBounds().contains(pointerX, pointerY))
          {
            mJoystickBeingConfigured = joystick;
            mJoystickBeingConfigured.onConfigureBegin(pointerX, pointerY);
            return true;
          }
        }
        break;
      case MotionEvent.ACTION_MOVE:
        if (mButtonBeingConfigured != null)
        {
          mButtonBeingConfigured.onConfigureMove(pointerX, pointerY);
          invalidate();
          return true;
        }
        if (mDpadBeingConfigured != null)
        {
          mDpadBeingConfigured.onConfigureMove(pointerX, pointerY);
          invalidate();
          return true;
        }
        if (mJoystickBeingConfigured != null)
        {
          mJoystickBeingConfigured.onConfigureMove(pointerX, pointerY);
          invalidate();
          return true;
        }
        break;
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_POINTER_UP:
        if (mButtonBeingConfigured != null)
        {
          saveControlPosition(mButtonBeingConfigured.getButtonId(), mButtonBeingConfigured.getBounds());
          mButtonBeingConfigured = null;
          return true;
        }
        if (mDpadBeingConfigured != null)
        {
          saveControlPosition(mDpadBeingConfigured.getButtonId(0), mDpadBeingConfigured.getBounds());
          mDpadBeingConfigured = null;
          return true;
        }
        if (mJoystickBeingConfigured != null)
        {
          saveControlPosition(mJoystickBeingConfigured.getButtonId(), mJoystickBeingConfigured.getBounds());
          mJoystickBeingConfigured = null;
          return true;
        }
        break;
    }

    return false;
  }

  public void onSensorChanged(float[] rotation)
  {
    if(mOverlaySensor != null)
    {
      mOverlaySensor.onSensorChanged(rotation);
    }
  }

  public void onAccuracyChanged(int accuracy)
  {
    if(mOverlaySensor == null)
    {
      mOverlaySensor = new InputOverlaySensor();
    }
    mOverlaySensor.onAccuracyChanged(accuracy);
  }

  private void addGameCubeOverlayControls()
  {
    int i = 0;
    int[][] buttons = new int[][]{
      { ButtonType.BUTTON_A, R.drawable.gcpad_a, R.drawable.gcpad_a_pressed },
      { ButtonType.BUTTON_B, R.drawable.gcpad_b, R.drawable.gcpad_b_pressed },
      { ButtonType.BUTTON_X, R.drawable.gcpad_x, R.drawable.gcpad_x_pressed },
      { ButtonType.BUTTON_Y, R.drawable.gcpad_y, R.drawable.gcpad_y_pressed },
      { ButtonType.BUTTON_Z, R.drawable.gcpad_z, R.drawable.gcpad_z_pressed },
      { ButtonType.BUTTON_START, R.drawable.gcpad_start, R.drawable.gcpad_start_pressed },
      { ButtonType.TRIGGER_L, R.drawable.gcpad_l, R.drawable.gcpad_l_pressed },
      { ButtonType.TRIGGER_R, R.drawable.gcpad_r, R.drawable.gcpad_r_pressed },
      { ButtonType.TRIGGER_L_ANALOG, R.drawable.classic_l, R.drawable.classic_l_pressed },
      { ButtonType.TRIGGER_R_ANALOG, R.drawable.classic_r, R.drawable.classic_r_pressed },
    };
    String prefId = "ToggleGc_";
    for(; i < buttons.length; ++i)
    {
      int id = buttons[i][0];
      int normal = buttons[i][1];
      int pressed = buttons[i][2];
      if (mPreferences.getBoolean(prefId + i, true))
      {
        mButtons.add(initializeOverlayButton(normal, pressed, id));
      }
    }

    if (mPreferences.getBoolean(prefId + (i++), true))
    {
      mDpads.add(initializeOverlayDpad(
        ButtonType.BUTTON_UP, ButtonType.BUTTON_DOWN,
        ButtonType.BUTTON_LEFT, ButtonType.BUTTON_RIGHT));
    }
    if (mPreferences.getBoolean(prefId + (i++), true))
    {
      mJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick,
        R.drawable.gcwii_joystick_pressed, ButtonType.STICK_MAIN));
    }
    if (mPreferences.getBoolean(prefId + (i++), true))
    {
      mJoysticks.add(initializeOverlayJoystick(R.drawable.gcpad_c,
        R.drawable.gcpad_c_pressed, ButtonType.STICK_C));
    }
  }

  private void addWiimoteOverlayControls()
  {
    int i = 0;
    int[][] buttons = new int[][]{
      { ButtonType.WIIMOTE_BUTTON_A, R.drawable.wiimote_a, R.drawable.wiimote_a_pressed },
      { ButtonType.WIIMOTE_BUTTON_B, R.drawable.wiimote_b, R.drawable.wiimote_b_pressed },
      { ButtonType.WIIMOTE_BUTTON_1, R.drawable.wiimote_one, R.drawable.wiimote_one_pressed },
      { ButtonType.WIIMOTE_BUTTON_2, R.drawable.wiimote_two, R.drawable.wiimote_two_pressed },
      { ButtonType.WIIMOTE_BUTTON_PLUS, R.drawable.wiimote_plus, R.drawable.wiimote_plus_pressed },
      { ButtonType.WIIMOTE_BUTTON_MINUS, R.drawable.wiimote_minus, R.drawable.wiimote_minus_pressed },
      { ButtonType.WIIMOTE_BUTTON_HOME, R.drawable.wiimote_home, R.drawable.wiimote_home_pressed },
      { ButtonType.HOTKEYS_UPRIGHT_TOGGLE, R.drawable.classic_x, R.drawable.classic_x_pressed },
      { ButtonType.WIIMOTE_TILT_TOGGLE, R.drawable.nunchuk_z, R.drawable.nunchuk_z_pressed },
    };
    int length = sControllerType == CONTROLLER_WIIREMOTE ? buttons.length : buttons.length - 1;
    String prefId = "ToggleWii_";
    for(; i < length; ++i)
    {
      int id = buttons[i][0];
      int normal = buttons[i][1];
      int pressed = buttons[i][2];
      if (mPreferences.getBoolean(prefId + i, true))
      {
        mButtons.add(initializeOverlayButton(normal, pressed, id));
      }
    }

    if (mPreferences.getBoolean(prefId + (i++), true))
    {
      if (sControllerType == CONTROLLER_WIINUNCHUK)
      {
        mDpads.add(initializeOverlayDpad(
          ButtonType.WIIMOTE_UP, ButtonType.WIIMOTE_DOWN,
          ButtonType.WIIMOTE_LEFT, ButtonType.WIIMOTE_RIGHT));
      }
      else
      {
        // Horizontal Wii Remote
        mDpads.add(initializeOverlayDpad(
          ButtonType.WIIMOTE_RIGHT, ButtonType.WIIMOTE_LEFT,
          ButtonType.WIIMOTE_UP, ButtonType.WIIMOTE_DOWN));
      }
    }

    if (sControllerType == CONTROLLER_WIINUNCHUK)
    {
      if (mPreferences.getBoolean(prefId + (i++), true))
      {
        mButtons.add(initializeOverlayButton(R.drawable.nunchuk_c, R.drawable.nunchuk_c_pressed,
          ButtonType.NUNCHUK_BUTTON_C));
      }
      if (mPreferences.getBoolean(prefId + (i++), true))
      {
        mButtons.add(initializeOverlayButton(R.drawable.nunchuk_z, R.drawable.nunchuk_z_pressed,
          ButtonType.NUNCHUK_BUTTON_Z));
      }
      if (mPreferences.getBoolean(prefId + (i++), true))
      {
        mJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick,
          R.drawable.gcwii_joystick_pressed, ButtonType.NUNCHUK_STICK));
      }
    }

    // joystick emulate
    if(sJoyStickSetting != JOYSTICK_EMULATE_NONE)
    {
      mJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick,
        R.drawable.gcwii_joystick_pressed, 0));
    }
  }

  private void addClassicOverlayControls()
  {
    int i = 0;
    int[][] buttons = new int[][]{
      { ButtonType.CLASSIC_BUTTON_A, R.drawable.classic_a, R.drawable.classic_a_pressed },
      { ButtonType.CLASSIC_BUTTON_B, R.drawable.classic_b, R.drawable.classic_b_pressed },
      { ButtonType.CLASSIC_BUTTON_X, R.drawable.classic_x, R.drawable.classic_x_pressed },
      { ButtonType.CLASSIC_BUTTON_Y, R.drawable.classic_y, R.drawable.classic_y_pressed },
      { ButtonType.WIIMOTE_BUTTON_1, R.drawable.wiimote_one, R.drawable.wiimote_one_pressed },
      { ButtonType.WIIMOTE_BUTTON_2, R.drawable.wiimote_two, R.drawable.wiimote_two_pressed },
      { ButtonType.CLASSIC_BUTTON_PLUS, R.drawable.wiimote_plus, R.drawable.wiimote_plus_pressed },
      { ButtonType.CLASSIC_BUTTON_MINUS, R.drawable.wiimote_minus, R.drawable.wiimote_minus_pressed },
      { ButtonType.CLASSIC_BUTTON_HOME, R.drawable.wiimote_home, R.drawable.wiimote_home_pressed },
      { ButtonType.CLASSIC_TRIGGER_L, R.drawable.classic_l, R.drawable.classic_l_pressed },
      { ButtonType.CLASSIC_TRIGGER_R, R.drawable.classic_r, R.drawable.classic_r_pressed },
      { ButtonType.CLASSIC_BUTTON_ZL, R.drawable.classic_zl, R.drawable.classic_zl_pressed },
      { ButtonType.CLASSIC_BUTTON_ZR, R.drawable.classic_zr, R.drawable.classic_zr_pressed },
    };
    String prefId = "ToggleClassic_";
    for(; i < buttons.length; ++i)
    {
      int id = buttons[i][0];
      int normal = buttons[i][1];
      int pressed = buttons[i][2];
      if (mPreferences.getBoolean(prefId + i, true))
      {
        mButtons.add(initializeOverlayButton(normal, pressed, id));
      }
    }

    if (mPreferences.getBoolean(prefId + (i++), true))
    {
      mDpads.add(initializeOverlayDpad(
        ButtonType.CLASSIC_DPAD_UP, ButtonType.CLASSIC_DPAD_DOWN,
        ButtonType.CLASSIC_DPAD_LEFT, ButtonType.CLASSIC_DPAD_RIGHT));
    }
    if (mPreferences.getBoolean(prefId + (i++), true))
    {
      mJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick,
        R.drawable.gcwii_joystick_pressed, ButtonType.CLASSIC_STICK_LEFT));
    }
    if (mPreferences.getBoolean(prefId + (i++), true))
    {
      mJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick,
        R.drawable.gcwii_joystick_pressed, ButtonType.CLASSIC_STICK_RIGHT));
    }

    // joystick emulate
    if(sJoyStickSetting != JOYSTICK_EMULATE_NONE)
    {
      mJoysticks.add(initializeOverlayJoystick(R.drawable.gcwii_joystick,
        R.drawable.gcwii_joystick_pressed, 0));
    }
  }

  public void refreshControls()
  {
    // Remove all the overlay buttons
    mButtons.clear();
    mDpads.clear();
    mJoysticks.clear();

    if(mPreferences.getBoolean("showInputOverlay", true))
    {
      if (EmulationActivity.get().isGameCubeGame() || sControllerType == CONTROLLER_GAMECUBE)
      {
        addGameCubeOverlayControls();
      }
      else if (sControllerType == CONTROLLER_CLASSIC)
      {
        addClassicOverlayControls();
      }
      else
      {
        addWiimoteOverlayControls();
      }
    }

    invalidate();
  }

  public void resetCurrentLayout()
  {
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
    Resources res = getResources();

    switch (getControllerType())
    {
      case CONTROLLER_GAMECUBE:
        gcDefaultOverlay(sPrefsEditor, res);
        break;
      case CONTROLLER_CLASSIC:
        wiiClassicDefaultOverlay(sPrefsEditor, res);
        break;
      case CONTROLLER_WIINUNCHUK:
        wiiNunchukDefaultOverlay(sPrefsEditor, res);
        break;
      case CONTROLLER_WIIREMOTE:
        wiiRemoteDefaultOverlay(sPrefsEditor, res);
        break;
    }

    sPrefsEditor.apply();
    refreshControls();
  }

  public void setTouchPointer(int type)
  {
    if(type > 0)
    {
      if(mOverlayPointer == null)
      {
        final DisplayMetrics dm = getContext().getResources().getDisplayMetrics();
        mOverlayPointer = new InputOverlayPointer(dm.widthPixels, dm.heightPixels,  dm.scaledDensity);
      }
      mOverlayPointer.setType(type);
    }
    else
    {
      mOverlayPointer = null;
    }
  }

  public void updateTouchPointer()
  {
    if(mOverlayPointer != null)
    {
      mOverlayPointer.updateTouchPointer();
    }
  }

  private void saveControlPosition(int buttonId, Rect bounds)
  {
    final Context context = getContext();
    final DisplayMetrics dm = context.getResources().getDisplayMetrics();
    final int controller = getControllerType();
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
    float x = (bounds.left + (bounds.right - bounds.left) / 2.0f) / dm.widthPixels * 2.0f - 1.0f;
    float y = (bounds.top + (bounds.bottom - bounds.top) / 2.0f) / dm.heightPixels * 2.0f - 1.0f;
    sPrefsEditor.putFloat(controller + "_" + buttonId + "_X", x);
    sPrefsEditor.putFloat(controller + "_" + buttonId + "_Y", y);
    sPrefsEditor.apply();
  }

  private int getControllerType()
  {
    return EmulationActivity.get().isGameCubeGame() ? CONTROLLER_GAMECUBE : sControllerType;
  }

  private InputOverlayDrawableButton initializeOverlayButton(int defaultResId, int pressedResId,
    int buttonId)
  {
    final Context context = getContext();
    // Resources handle for fetching the initial Drawable resource.
    final Resources res = context.getResources();
    // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
    final int controller = getControllerType();
    // Decide scale based on button ID and user preference
    float scale;

    switch (buttonId)
    {
      case ButtonType.BUTTON_A:
      case ButtonType.WIIMOTE_BUTTON_B:
      case ButtonType.NUNCHUK_BUTTON_Z:
      case ButtonType.BUTTON_X:
      case ButtonType.BUTTON_Y:
      case ButtonType.TRIGGER_L_ANALOG:
      case ButtonType.TRIGGER_R_ANALOG:
        scale = 0.17f;
        break;
      case ButtonType.BUTTON_Z:
      case ButtonType.TRIGGER_L:
      case ButtonType.TRIGGER_R:
      case ButtonType.CLASSIC_BUTTON_ZL:
      case ButtonType.CLASSIC_BUTTON_ZR:
      case ButtonType.WIIMOTE_TILT_TOGGLE:
        scale = 0.18f;
        break;
      case ButtonType.BUTTON_START:
        scale = 0.075f;
        break;
      case ButtonType.WIIMOTE_BUTTON_1:
      case ButtonType.WIIMOTE_BUTTON_2:
      case ButtonType.WIIMOTE_BUTTON_PLUS:
      case ButtonType.WIIMOTE_BUTTON_MINUS:
        scale = 0.075f;
        if(controller == CONTROLLER_WIIREMOTE)
          scale = 0.125f;
        break;
      case ButtonType.WIIMOTE_BUTTON_HOME:
      case ButtonType.CLASSIC_BUTTON_PLUS:
      case ButtonType.CLASSIC_BUTTON_MINUS:
      case ButtonType.CLASSIC_BUTTON_HOME:
        scale = 0.075f;
        break;
      case ButtonType.HOTKEYS_UPRIGHT_TOGGLE:
        scale = 0.0675f;
        break;
      case ButtonType.CLASSIC_TRIGGER_L:
      case ButtonType.CLASSIC_TRIGGER_R:
        scale = 0.22f;
        break;
      case ButtonType.WIIMOTE_BUTTON_A:
        scale = 0.14f;
        break;
      case ButtonType.NUNCHUK_BUTTON_C:
        scale = 0.15f;
        break;
      default:
        scale = 0.125f;
        break;
    }

    scale *= sControllerScale + 50;
    scale /= 100;

    // Initialize the InputOverlayDrawableButton.
    Bitmap defaultBitmap = resizeBitmap(BitmapFactory.decodeResource(res, defaultResId), scale);
    Bitmap pressedBitmap = resizeBitmap(BitmapFactory.decodeResource(res, pressedResId), scale);
    InputOverlayDrawableButton overlay = new InputOverlayDrawableButton(
      new BitmapDrawable(res, defaultBitmap), new BitmapDrawable(res, pressedBitmap) , buttonId);

    // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
    // These were set in the input overlay configuration menu.
    float x = mPreferences.getFloat(controller + "_" + buttonId + "_X", 0f);
    float y = mPreferences.getFloat(controller + "_" + buttonId + "_Y", 0.5f);

    int width = defaultBitmap.getWidth();
    int height = defaultBitmap.getHeight();
    DisplayMetrics dm = res.getDisplayMetrics();
    int drawableX = (int) ((dm.widthPixels / 2.0f) * (1.0f + x) - width / 2.0f);
    int drawableY = (int) ((dm.heightPixels / 2.0f) * (1.0f + y) - height / 2.0f);
    // Now set the bounds for the InputOverlayDrawableButton.
    // This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
    overlay.setBounds(new Rect(drawableX, drawableY, drawableX + width, drawableY + height));

    // Need to set the image's position
    overlay.setPosition(drawableX, drawableY);
    overlay.setAlpha((sControllerAlpha * 255) / 100);

    return overlay;
  }

  private InputOverlayDrawableDpad initializeOverlayDpad(int buttonUp, int buttonDown,
                                                         int buttonLeft, int buttonRight)
  {
    final int defaultResId = R.drawable.gcwii_dpad;
    final int pressedOneDirectionResId = R.drawable.gcwii_dpad_pressed_one_direction;
    final int pressedTwoDirectionsResId = R.drawable.gcwii_dpad_pressed_two_directions;
    // Resources handle for fetching the initial Drawable resource.
    final Resources res = getContext().getResources();
    // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableDpad.
    final int controller = getControllerType();

    // Decide scale based on button ID and user preference
    float scale = controller == CONTROLLER_WIIREMOTE ? 0.335f : 0.275f;
    scale *= sControllerScale + 50;
    scale /= 100;

    // Initialize the InputOverlayDrawableDpad.
    Bitmap defaultBitmap = resizeBitmap(BitmapFactory.decodeResource(res, defaultResId), scale);
    Bitmap onePressedBitmap =
      resizeBitmap(BitmapFactory.decodeResource(res, pressedOneDirectionResId), scale);
    Bitmap twoPressedBitmap =
      resizeBitmap(BitmapFactory.decodeResource(res, pressedTwoDirectionsResId), scale);
    InputOverlayDrawableDpad overlay = new InputOverlayDrawableDpad(
      new BitmapDrawable(res, defaultBitmap),
      new BitmapDrawable(res, onePressedBitmap), new BitmapDrawable(res, twoPressedBitmap),
      buttonUp, buttonDown, buttonLeft, buttonRight);

    // The X and Y coordinates of the InputOverlayDrawableDpad on the InputOverlay.
    // These were set in the input overlay configuration menu.
    float x = mPreferences.getFloat(controller + "_" + buttonUp + "_X", 0f);
    float y = mPreferences.getFloat(controller + "_" + buttonUp + "_Y", 0.5f);

    int width = defaultBitmap.getWidth();
    int height = defaultBitmap.getHeight();
    final DisplayMetrics dm = res.getDisplayMetrics();
    int drawableX = (int) ((dm.widthPixels / 2.0f) * (1.0f + x) - width / 2.0f);
    int drawableY = (int) ((dm.heightPixels / 2.0f) * (1.0f + y) - height / 2.0f);
    // Now set the bounds for the InputOverlayDrawableDpad.
    // This will dictate where on the screen (and the what the size) the InputOverlayDrawableDpad will be.
    overlay.setBounds(new Rect(drawableX, drawableY, drawableX + width, drawableY + height));

    // Need to set the image's position
    overlay.setPosition(drawableX, drawableY);
    overlay.setAlpha((sControllerAlpha * 255) / 100);

    return overlay;
  }

  private InputOverlayDrawableJoystick initializeOverlayJoystick(int defaultResInner,
                                                                 int pressedResInner, int joystick)
  {
    final Context context = getContext();
    // Resources handle for fetching the initial Drawable resource.
    final Resources res = context.getResources();
    // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableJoystick.
    final int controller = getControllerType();
    // Decide scale based on user preference
    float scale = 0.275f * (sControllerScale + 50) / 100;

    // Initialize the InputOverlayDrawableJoystick.
    final int resOuter = R.drawable.gcwii_joystick_range;
    Bitmap bitmapOuter = resizeBitmap(BitmapFactory.decodeResource(res, resOuter), scale);
    Bitmap bitmapInnerDefault = BitmapFactory.decodeResource(res, defaultResInner);
    Bitmap bitmapInnerPressed = BitmapFactory.decodeResource(res, pressedResInner);

    // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
    // These were set in the input overlay configuration menu.
    float x = mPreferences.getFloat(controller + "_" + joystick + "_X", -0.3f);
    float y = mPreferences.getFloat(controller + "_" + joystick + "_Y", 0.3f);

    // Decide inner scale based on joystick ID
    float innerScale = joystick == ButtonType.STICK_C ? 1.833f : 1.375f;

    // Now set the bounds for the InputOverlayDrawableJoystick.
    // This will dictate where on the screen (and the what the size) the InputOverlayDrawableJoystick will be.
    int outerSize = bitmapOuter.getWidth();
    final DisplayMetrics dm = res.getDisplayMetrics();
    int drawableX = (int) ((dm.widthPixels / 2.0f) * (1.0f + x) - outerSize / 2.0f);
    int drawableY = (int) ((dm.heightPixels / 2.0f) * (1.0f + y) - outerSize / 2.0f);

    Rect outerRect = new Rect(drawableX, drawableY, drawableX + outerSize, drawableY + outerSize);
    Rect innerRect = new Rect(0, 0, (int) (outerSize / innerScale), (int) (outerSize / innerScale));

    // Send the drawableId to the joystick so it can be referenced when saving control position.
    final InputOverlayDrawableJoystick overlay = new InputOverlayDrawableJoystick(
      new BitmapDrawable(res, bitmapOuter), new BitmapDrawable(res, bitmapOuter),
      new BitmapDrawable(res, bitmapInnerDefault), new BitmapDrawable(res, bitmapInnerPressed),
      outerRect, innerRect, joystick);

    // Need to set the image's position
    overlay.setPosition(drawableX, drawableY);
    overlay.setAlpha((sControllerAlpha * 255) / 100);

    return overlay;
  }

  private Bitmap resizeBitmap(Bitmap bitmap, float scale)
  {
    // Determine the button size based on the smaller screen dimension.
    // This makes sure the buttons are the same size in both portrait and landscape.
    Resources res = getContext().getResources();
    DisplayMetrics dm = res.getDisplayMetrics();
    int dimension = (int) (Math.min(dm.widthPixels, dm.heightPixels) * scale);
    return Bitmap.createScaledBitmap(bitmap, dimension, dimension, true);
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
    SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
    Resources res = getResources();

    // GameCube
    gcDefaultOverlay(sPrefsEditor, res);
    // Wii Nunchuk
    wiiNunchukDefaultOverlay(sPrefsEditor, res);
    // Wii Remote
    wiiRemoteDefaultOverlay(sPrefsEditor, res);
    // Wii Classic
    wiiClassicDefaultOverlay(sPrefsEditor, res);

    sPrefsEditor.putBoolean(CONTROL_INIT_PREF_KEY, true);
    sPrefsEditor.apply();
  }

  private void gcDefaultOverlay(SharedPreferences.Editor sPrefsEditor, Resources res)
  {
    final int controller = CONTROLLER_GAMECUBE;
    int[][] buttons = new int[][]{
      { ButtonType.BUTTON_A, R.integer.BUTTON_A_X, R.integer.BUTTON_A_Y },
      { ButtonType.BUTTON_B, R.integer.BUTTON_B_X, R.integer.BUTTON_B_Y },
      { ButtonType.BUTTON_X, R.integer.BUTTON_X_X, R.integer.BUTTON_X_Y },
      { ButtonType.BUTTON_Y, R.integer.BUTTON_Y_X, R.integer.BUTTON_Y_Y },
      { ButtonType.BUTTON_Z, R.integer.BUTTON_Z_X, R.integer.BUTTON_Z_Y },
      { ButtonType.BUTTON_UP, R.integer.BUTTON_UP_X, R.integer.BUTTON_UP_Y },
      { ButtonType.TRIGGER_L, R.integer.TRIGGER_L_X, R.integer.TRIGGER_L_Y },
      { ButtonType.TRIGGER_R, R.integer.TRIGGER_R_X, R.integer.TRIGGER_R_Y },
      { ButtonType.TRIGGER_L_ANALOG, R.integer.TRIGGER_L_ANALOG_X, R.integer.TRIGGER_L_ANALOG_Y },
      { ButtonType.TRIGGER_R_ANALOG, R.integer.TRIGGER_R_ANALOG_X, R.integer.TRIGGER_R_ANALOG_Y },
      { ButtonType.BUTTON_START, R.integer.BUTTON_START_X, R.integer.BUTTON_START_Y },
      { ButtonType.STICK_C, R.integer.STICK_C_X, R.integer.STICK_C_Y },
      { ButtonType.STICK_MAIN, R.integer.STICK_MAIN_X, R.integer.STICK_MAIN_Y },
    };

    for(int i = 0; i < buttons.length; ++i)
    {
      int id = buttons[i][0];
      int x = buttons[i][1];
      int y = buttons[i][2];
      sPrefsEditor.putFloat(controller + "_" + id + "_X", res.getInteger(x) / 100.0f);
      sPrefsEditor.putFloat(controller + "_" + id + "_Y", res.getInteger(y) / 100.0f);
    }
  }

  private void wiiNunchukDefaultOverlay(SharedPreferences.Editor sPrefsEditor, Resources res)
  {
    final int controller = CONTROLLER_WIINUNCHUK;
    int[][] buttons = new int[][]{
      { ButtonType.WIIMOTE_BUTTON_A, R.integer.WIIMOTE_BUTTON_A_X, R.integer.WIIMOTE_BUTTON_A_Y },
      { ButtonType.WIIMOTE_BUTTON_B, R.integer.WIIMOTE_BUTTON_B_X, R.integer.WIIMOTE_BUTTON_B_Y },
      { ButtonType.WIIMOTE_BUTTON_1, R.integer.WIIMOTE_BUTTON_1_X, R.integer.WIIMOTE_BUTTON_1_Y },
      { ButtonType.WIIMOTE_BUTTON_2, R.integer.WIIMOTE_BUTTON_2_X, R.integer.WIIMOTE_BUTTON_2_Y },
      { ButtonType.NUNCHUK_BUTTON_Z, R.integer.NUNCHUK_BUTTON_Z_X, R.integer.NUNCHUK_BUTTON_Z_Y },
      { ButtonType.NUNCHUK_BUTTON_C, R.integer.NUNCHUK_BUTTON_C_X, R.integer.NUNCHUK_BUTTON_C_Y },
      { ButtonType.WIIMOTE_BUTTON_MINUS, R.integer.WIIMOTE_BUTTON_MINUS_X, R.integer.WIIMOTE_BUTTON_MINUS_Y },
      { ButtonType.WIIMOTE_BUTTON_PLUS, R.integer.WIIMOTE_BUTTON_PLUS_X, R.integer.WIIMOTE_BUTTON_PLUS_Y },
      { ButtonType.WIIMOTE_BUTTON_HOME, R.integer.WIIMOTE_BUTTON_HOME_X, R.integer.WIIMOTE_BUTTON_HOME_Y },
      { ButtonType.WIIMOTE_UP, R.integer.WIIMOTE_UP_X, R.integer.WIIMOTE_UP_Y },
      { ButtonType.NUNCHUK_STICK, R.integer.NUNCHUK_STICK_X, R.integer.NUNCHUK_STICK_Y },
      { ButtonType.WIIMOTE_RIGHT, R.integer.WIIMOTE_RIGHT_X, R.integer.WIIMOTE_RIGHT_Y },
      { ButtonType.HOTKEYS_UPRIGHT_TOGGLE, R.integer.WIIMOTE_BUTTON_UPRIGHT_TOGGLE_X, R.integer.WIIMOTE_BUTTON_UPRIGHT_TOGGLE_Y },
    };

    for(int i = 0; i < buttons.length; ++i)
    {
      int id = buttons[i][0];
      int x = buttons[i][1];
      int y = buttons[i][2];
      sPrefsEditor.putFloat(controller + "_" + id + "_X", res.getInteger(x) / 100.0f);
      sPrefsEditor.putFloat(controller + "_" + id + "_Y", res.getInteger(y) / 100.0f);
    }
  }

  private void wiiRemoteDefaultOverlay(SharedPreferences.Editor sPrefsEditor, Resources res)
  {
    final int controller = CONTROLLER_WIIREMOTE;
    int[][] buttons = new int[][]{
      { ButtonType.WIIMOTE_BUTTON_A, R.integer.WIIMOTE_BUTTON_A_X, R.integer.WIIMOTE_BUTTON_A_Y },
      { ButtonType.WIIMOTE_BUTTON_B, R.integer.WIIMOTE_BUTTON_B_X, R.integer.WIIMOTE_BUTTON_B_Y },
      { ButtonType.WIIMOTE_BUTTON_1, R.integer.WIIMOTE_BUTTON_1_X, R.integer.WIIMOTE_BUTTON_1_Y },
      { ButtonType.WIIMOTE_BUTTON_2, R.integer.WIIMOTE_BUTTON_2_X, R.integer.WIIMOTE_BUTTON_2_Y },
      { ButtonType.WIIMOTE_BUTTON_MINUS, R.integer.WIIMOTE_BUTTON_MINUS_X, R.integer.WIIMOTE_BUTTON_MINUS_Y },
      { ButtonType.WIIMOTE_BUTTON_PLUS, R.integer.WIIMOTE_BUTTON_PLUS_X, R.integer.WIIMOTE_BUTTON_PLUS_Y },
      { ButtonType.WIIMOTE_BUTTON_HOME, R.integer.WIIMOTE_BUTTON_HOME_X, R.integer.WIIMOTE_BUTTON_HOME_Y },
      { ButtonType.WIIMOTE_RIGHT, R.integer.WIIMOTE_RIGHT_X, R.integer.WIIMOTE_RIGHT_Y },
      { ButtonType.HOTKEYS_UPRIGHT_TOGGLE, R.integer.WIIMOTE_BUTTON_UPRIGHT_TOGGLE_X, R.integer.WIIMOTE_BUTTON_UPRIGHT_TOGGLE_Y },
      { ButtonType.WIIMOTE_TILT_TOGGLE, R.integer.WIIMOTE_BUTTON_TILT_TOGGLE_X, R.integer.WIIMOTE_BUTTON_TILT_TOGGLE_Y },
    };

    for(int i = 0; i < buttons.length; ++i)
    {
      int id = buttons[i][0];
      int x = buttons[i][1];
      int y = buttons[i][2];
      sPrefsEditor.putFloat(controller + "_" + id + "_X", res.getInteger(x) / 100.0f);
      sPrefsEditor.putFloat(controller + "_" + id + "_Y", res.getInteger(y) / 100.0f);
    }
  }

  private void wiiClassicDefaultOverlay(SharedPreferences.Editor sPrefsEditor, Resources res)
  {
    final int controller = CONTROLLER_CLASSIC;
    int[][] buttons = new int[][]{
      { ButtonType.CLASSIC_BUTTON_A, R.integer.CLASSIC_BUTTON_A_X, R.integer.CLASSIC_BUTTON_A_Y },
      { ButtonType.CLASSIC_BUTTON_B, R.integer.CLASSIC_BUTTON_B_X, R.integer.CLASSIC_BUTTON_B_Y },
      { ButtonType.CLASSIC_BUTTON_X, R.integer.CLASSIC_BUTTON_X_X, R.integer.CLASSIC_BUTTON_X_Y },
      { ButtonType.CLASSIC_BUTTON_Y, R.integer.CLASSIC_BUTTON_Y_X, R.integer.CLASSIC_BUTTON_Y_Y },
      { ButtonType.WIIMOTE_BUTTON_1, R.integer.CLASSIC_BUTTON_1_X, R.integer.CLASSIC_BUTTON_1_Y },
      { ButtonType.WIIMOTE_BUTTON_2, R.integer.CLASSIC_BUTTON_2_X, R.integer.CLASSIC_BUTTON_2_Y },
      { ButtonType.CLASSIC_BUTTON_MINUS, R.integer.CLASSIC_BUTTON_MINUS_X, R.integer.CLASSIC_BUTTON_MINUS_Y },
      { ButtonType.CLASSIC_BUTTON_PLUS, R.integer.CLASSIC_BUTTON_PLUS_X, R.integer.CLASSIC_BUTTON_PLUS_Y },
      { ButtonType.CLASSIC_BUTTON_HOME, R.integer.CLASSIC_BUTTON_HOME_X, R.integer.CLASSIC_BUTTON_HOME_Y },
      { ButtonType.CLASSIC_BUTTON_ZL, R.integer.CLASSIC_BUTTON_ZL_X, R.integer.CLASSIC_BUTTON_ZL_Y },
      { ButtonType.CLASSIC_BUTTON_ZR, R.integer.CLASSIC_BUTTON_ZR_X, R.integer.CLASSIC_BUTTON_ZR_Y },
      { ButtonType.CLASSIC_DPAD_UP, R.integer.CLASSIC_DPAD_UP_X, R.integer.CLASSIC_DPAD_UP_Y },
      { ButtonType.CLASSIC_STICK_LEFT, R.integer.CLASSIC_STICK_LEFT_X, R.integer.CLASSIC_STICK_LEFT_Y },
      { ButtonType.CLASSIC_STICK_RIGHT, R.integer.CLASSIC_STICK_RIGHT_X, R.integer.CLASSIC_STICK_RIGHT_Y },
      { ButtonType.CLASSIC_TRIGGER_L, R.integer.CLASSIC_TRIGGER_L_X, R.integer.CLASSIC_TRIGGER_L_Y },
      { ButtonType.CLASSIC_TRIGGER_R, R.integer.CLASSIC_TRIGGER_R_X, R.integer.CLASSIC_TRIGGER_R_Y },
    };

    for(int i = 0; i < buttons.length; ++i)
    {
      int id = buttons[i][0];
      int x = buttons[i][1];
      int y = buttons[i][2];
      sPrefsEditor.putFloat(controller + "_" + id + "_X", res.getInteger(x) / 100.0f);
      sPrefsEditor.putFloat(controller + "_" + id + "_Y", res.getInteger(y) / 100.0f);
    }
  }
}
