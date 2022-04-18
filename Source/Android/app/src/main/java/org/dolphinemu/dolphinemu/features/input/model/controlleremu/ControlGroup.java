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
  public static final int TYPE_OTHER = 0;
  public static final int TYPE_STICK = 1;
  public static final int TYPE_MIXED_TRIGGERS = 2;
  public static final int TYPE_BUTTONS = 3;
  public static final int TYPE_FORCE = 4;
  public static final int TYPE_ATTACHMENTS = 5;
  public static final int TYPE_TILT = 6;
  public static final int TYPE_CURSOR = 7;
  public static final int TYPE_TRIGGERS = 8;
  public static final int TYPE_SLIDER = 9;
  public static final int TYPE_SHAKE = 10;
  public static final int TYPE_IMU_ACCELEROMETER = 11;
  public static final int TYPE_IMU_GYROSCOPE = 12;
  public static final int TYPE_IMU_CURSOR = 13;

  public static final int DEFAULT_ENABLED_ALWAYS = 0;
  public static final int DEFAULT_ENABLED_YES = 1;
  public static final int DEFAULT_ENABLED_NO = 2;

  @Keep
  private final long mPointer;

  @Keep
  private ControlGroup(long pointer)
  {
    mPointer = pointer;
  }

  public native String getUiName();

  public native int getGroupType();

  public native int getDefaultEnabledValue();

  public native boolean getEnabled();

  public native void setEnabled(boolean value);

  public native int getControlCount();

  public native Control getControl(int i);

  public native int getNumericSettingCount();

  public native NumericSetting getNumericSetting(int i);

  /**
   * If getGroupType returns TYPE_ATTACHMENTS, this returns the attachment selection setting.
   * Otherwise, undefined behavior!
   */
  public native NumericSetting getAttachmentSetting();
}
