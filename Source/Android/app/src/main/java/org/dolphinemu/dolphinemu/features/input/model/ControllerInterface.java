// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.Handler;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.utils.LooperThread;

/**
 * This class interfaces with the native ControllerInterface,
 * which is where the emulator core gets inputs from.
 */
public final class ControllerInterface
{
  private static final class InputDeviceListener implements InputManager.InputDeviceListener
  {
    @Override
    public void onInputDeviceAdded(int deviceId)
    {
      // Simple implementation for now. We could do something fancier if we wanted to.
      refreshDevices();
    }

    @Override
    public void onInputDeviceRemoved(int deviceId)
    {
      // Simple implementation for now. We could do something fancier if we wanted to.
      refreshDevices();
    }

    @Override
    public void onInputDeviceChanged(int deviceId)
    {
      // Simple implementation for now. We could do something fancier if we wanted to.
      refreshDevices();
    }
  }

  private static InputDeviceListener mInputDeviceListener;
  private static LooperThread mLooperThread;

  /**
   * Activities which want to pass on inputs to native code
   * should call this in their own dispatchKeyEvent method.
   *
   * @return true if the emulator core seems to be interested in this event.
   * false if the event should be passed on to the default dispatchKeyEvent.
   */
  public static native boolean dispatchKeyEvent(KeyEvent event);

  /**
   * Activities which want to pass on inputs to native code
   * should call this in their own dispatchGenericMotionEvent method.
   *
   * @return true if the emulator core seems to be interested in this event.
   * false if the event should be passed on to the default dispatchGenericMotionEvent.
   */
  public static native boolean dispatchGenericMotionEvent(MotionEvent event);

  /**
   * {@link DolphinSensorEventListener} calls this for each axis of a received SensorEvent.
   *
   * @return true if the emulator core seems to be interested in this event.
   * false if the sensor can be suspended to save battery.
   */
  public static native boolean dispatchSensorEvent(String deviceQualifier, String axisName,
          float value);

  /**
   * Called when a sensor is suspended or unsuspended.
   *
   * @param deviceQualifier A string used by native code for uniquely identifying devices.
   * @param axisNames       The name of all axes for the sensor.
   * @param suspended       Whether the sensor is now suspended.
   */
  public static native void notifySensorSuspendedState(String deviceQualifier, String[] axisNames,
          boolean suspended);

  /**
   * Rescans for input devices.
   */
  public static native void refreshDevices();

  public static native String[] getAllDeviceStrings();

  @Nullable
  public static native CoreDevice getDevice(String deviceString);

  @Keep
  private static void registerInputDeviceListener()
  {
    if (mLooperThread == null)
    {
      mLooperThread = new LooperThread("Hotplug thread");
      mLooperThread.start();
    }

    if (mInputDeviceListener == null)
    {
      InputManager im = (InputManager)
              DolphinApplication.getAppContext().getSystemService(Context.INPUT_SERVICE);

      mInputDeviceListener = new InputDeviceListener();
      im.registerInputDeviceListener(mInputDeviceListener, new Handler(mLooperThread.getLooper()));
    }
  }

  @Keep
  private static void unregisterInputDeviceListener()
  {
    if (mInputDeviceListener != null)
    {
      InputManager im = (InputManager)
              DolphinApplication.getAppContext().getSystemService(Context.INPUT_SERVICE);

      im.unregisterInputDeviceListener(mInputDeviceListener);
      mInputDeviceListener = null;
    }
  }

  @Keep @NonNull
  private static DolphinVibratorManager getVibratorManager(InputDevice device)
  {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
    {
      return new DolphinVibratorManagerPassthrough(device.getVibratorManager());
    }
    else
    {
      return new DolphinVibratorManagerCompat(device.getVibrator());
    }
  }

  @Keep @NonNull
  private static DolphinVibratorManager getSystemVibratorManager()
  {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
    {
      VibratorManager vibratorManager = (VibratorManager)
              DolphinApplication.getAppContext().getSystemService(Context.VIBRATOR_MANAGER_SERVICE);

      if (vibratorManager != null)
        return new DolphinVibratorManagerPassthrough(vibratorManager);
    }

    Vibrator vibrator = (Vibrator)
            DolphinApplication.getAppContext().getSystemService(Context.VIBRATOR_SERVICE);

    return new DolphinVibratorManagerCompat(vibrator);
  }

  @Keep
  private static void vibrate(@NonNull Vibrator vibrator)
  {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
    {
      vibrator.vibrate(VibrationEffect.createOneShot(100, VibrationEffect.DEFAULT_AMPLITUDE));
    }
    else
    {
      vibrator.vibrate(100);
    }
  }
}
