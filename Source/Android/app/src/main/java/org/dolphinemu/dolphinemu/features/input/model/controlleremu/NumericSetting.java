// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu;

import androidx.annotation.Keep;

/**
 * Represents a C++ ControllerEmu::NumericSetting.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
public class NumericSetting
{
  public static final int TYPE_INT = 0;
  public static final int TYPE_DOUBLE = 1;
  public static final int TYPE_BOOLEAN = 2;

  @Keep
  private final long mPointer;

  @Keep
  private NumericSetting(long pointer)
  {
    mPointer = pointer;
  }

  /**
   * @return The name used in the UI.
   */
  public native String getUiName();

  /**
   * @return A string applied to the number in the UI (unit of measure).
   */
  public native String getUiSuffix();

  /**
   * @return Detailed description of the setting.
   */
  public native String getUiDescription();

  /**
   * @return TYPE_INT, TYPE_DOUBLE or TYPE_BOOLEAN
   */
  public native int getType();

  public native ControlReference getControlReference();

  /**
   * If the type is TYPE_INT, gets the current value. Otherwise, undefined behavior!
   */
  public native int getIntValue();

  /**
   * If the type is TYPE_INT, sets the current value. Otherwise, undefined behavior!
   */
  public native void setIntValue(int value);

  /**
   * If the type is TYPE_INT, gets the default value. Otherwise, undefined behavior!
   */
  public native int getIntDefaultValue();

  /**
   * If the type is TYPE_DOUBLE, gets the current value. Otherwise, undefined behavior!
   */
  public native double getDoubleValue();

  /**
   * If the type is TYPE_DOUBLE, sets the current value. Otherwise, undefined behavior!
   */
  public native void setDoubleValue(double value);

  /**
   * If the type is TYPE_DOUBLE, gets the default value. Otherwise, undefined behavior!
   */
  public native double getDoubleDefaultValue();

  /**
   * If the type is TYPE_DOUBLE, returns the minimum valid value. Otherwise, undefined behavior!
   */
  public native double getDoubleMin();

  /**
   * If the type is TYPE_DOUBLE, returns the maximum valid value. Otherwise, undefined behavior!
   */
  public native double getDoubleMax();

  /**
   * If the type is TYPE_BOOLEAN, gets the current value. Otherwise, undefined behavior!
   */
  public native boolean getBooleanValue();

  /**
   * If the type is TYPE_BOOLEAN, sets the current value. Otherwise, undefined behavior!
   */
  public native void setBooleanValue(boolean value);

  /**
   * If the type is TYPE_BOOLEAN, gets the default value. Otherwise, undefined behavior!
   */
  public native boolean getBooleanDefaultValue();
}
