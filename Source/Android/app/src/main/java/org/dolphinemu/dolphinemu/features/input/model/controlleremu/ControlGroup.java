// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu;

import androidx.annotation.Keep;

/**
 * Represents a C++ ControllerEmu::ControlGroup.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
public class ControlGroup
{
  @Keep
  private final long mPointer;

  @Keep
  private ControlGroup(long pointer)
  {
    mPointer = pointer;
  }

  public native String getUiName();

  public native int getControlCount();

  public native Control getControl(int i);

  public native int getNumericSettingCount();

  public native NumericSetting getNumericSetting(int i);
}
