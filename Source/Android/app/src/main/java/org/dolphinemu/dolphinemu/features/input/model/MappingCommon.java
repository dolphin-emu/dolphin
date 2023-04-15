// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import androidx.annotation.NonNull;

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController;

public final class MappingCommon
{
  private MappingCommon()
  {
  }

  /**
   * Waits until the user presses one or more inputs or until a timeout,
   * then returns the pressed inputs.
   *
   * When this is being called, a separate thread must be calling ControllerInterface's
   * dispatchKeyEvent and dispatchGenericMotionEvent, otherwise no inputs will be registered.
   *
   * @param controller The device to detect inputs from.
   * @param allDevices Whether to also detect inputs from devices other than the specified one.
   * @return The input(s) pressed by the user in the form of an InputCommon expression,
   * or an empty string if there were no inputs.
   */
  public static native String detectInput(@NonNull EmulatedController controller,
          boolean allDevices);

  public static native String getExpressionForControl(String control, String device,
          String defaultDevice);

  public static native void save();
}
