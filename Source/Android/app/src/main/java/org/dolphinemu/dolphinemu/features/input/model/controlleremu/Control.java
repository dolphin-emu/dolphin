// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu;

import androidx.annotation.Keep;

/**
 * Represents a C++ ControllerEmu::Control.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
public class Control
{
  @Keep
  private final long mPointer;

  @Keep
  private Control(long pointer)
  {
    mPointer = pointer;
  }

  public native String getUiName();

  public native ControlReference getControlReference();
}
