// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.annotation.Keep;

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
   */
  public static native void dispatchSensorEvent(String axisName, float value);

  /**
   * Enables delivering sensor events to native code.
   *
   * @param requester The activity or other component which is requesting sensor events to be
   *                  delivered.
   */
  public static native void enableSensorEvents(SensorEventRequester requester);

  /**
   * Disables delivering sensor events to native code.
   *
   * Calling this when sensor events are no longer needed will save battery.
   */
  public static native void disableSensorEvents();

  /**
   * Rescans for input devices.
   */
  public static native void refreshDevices();

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
}
