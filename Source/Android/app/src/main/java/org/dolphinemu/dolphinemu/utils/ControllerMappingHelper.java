// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import java.util.Arrays;

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
    // 0x0b20 is Firmware > 5.1 where the scaling is correct.
    else if (isXboxOneWireless(inputDevice) && inputDevice.getProductId() != 0x0b20)
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

  private static final int[] XboxProductIDs = {
          // Xbox One (Rev. 1)
          0x2d1,
          // Xbox One (Rev. 2)
          0x2dd,
          // Xbox One (Rev. 3)
          0x2e0,
          // Xbox One Elite (Wired)
          0x2e3,
          // Xbox One S Controller
          0x2ea,
          // Xbox One S Controller (Bluetooth)
          0x2fd,
          // Xbox One Wireless Controller (model 1914)
          0x0b12,
          // Xbox One Wireless Controller (Firmware > 5.1)
          0x0b20
  };

  private static boolean isXboxOneWireless(InputDevice inputDevice)
  {
    return inputDevice.getVendorId() == 0x45e &&
            Arrays.asList(XboxProductIDs).contains(inputDevice.getProductId());
  }
}
