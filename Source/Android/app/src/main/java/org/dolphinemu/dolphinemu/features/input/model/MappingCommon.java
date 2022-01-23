// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

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
   * @return The input(s) pressed by the user in the form of an InputCommon expression,
   * or an empty string if there were no inputs.
   */
  public static native String detectInput();

  public static native void save();
}
