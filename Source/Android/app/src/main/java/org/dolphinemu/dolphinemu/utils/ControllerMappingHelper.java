package org.dolphinemu.dolphinemu.utils;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

/**
 * Some controllers have incorrect mappings. This class has special-case fixes for them.
 */
public class ControllerMappingHelper
{
  /**
   * Some controllers report extra button presses that can be ignored.
   */
  public static boolean shouldKeyBeIgnored(InputDevice inputDevice, int keyCode)
  {
    if (isDualShock4(inputDevice))
    {
      // The two analog triggers generate analog motion events as well as a keycode.
      // We always prefer to use the analog values, so throw away the button press
      return keyCode == KeyEvent.KEYCODE_BUTTON_L2 || keyCode == KeyEvent.KEYCODE_BUTTON_R2;
    }
    return false;
  }

  /**
   * Scale an axis to be zero-centered with a proper range.
   */
  public static float scaleAxis(InputDevice inputDevice, int axis, float value)
  {
    if (isDualShock4(inputDevice))
    {
      // Android doesn't have correct mappings for this controller's triggers. It reports them
      // as RX & RY, centered at -1.0, and with a range of [-1.0, 1.0]
      // Scale them to properly zero-centered with a range of [0.0, 1.0].
      if (axis == MotionEvent.AXIS_RX || axis == MotionEvent.AXIS_RY)
      {
        return (value + 1) / 2.0f;
      }
    }
    else if (isXboxOneWireless(inputDevice))
    {
      // Same as the DualShock 4, the mappings are missing.
      if (axis == MotionEvent.AXIS_Z || axis == MotionEvent.AXIS_RZ)
      {
        return (value + 1) / 2.0f;
      }
    }
    return value;
  }

  private static boolean isDualShock4(InputDevice inputDevice)
  {
    // Sony DualShock 4 controller
    return inputDevice.getVendorId() == 0x54c && inputDevice.getProductId() == 0x9cc;
  }

  private static boolean isXboxOneWireless(InputDevice inputDevice)
  {
    // Microsoft Xbox One controller
    return inputDevice.getVendorId() == 0x45e && inputDevice.getProductId() == 0x2e0;
  }
}
