// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu;

import androidx.annotation.Keep;

/**
 * Represents a C++ ControllerEmu::EmulatedController.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
public class EmulatedController
{
  @Keep
  private final long mPointer;

  @Keep
  private EmulatedController(long pointer)
  {
    mPointer = pointer;
  }

  public native String getDefaultDevice();

  public native void setDefaultDevice(String device);

  public native int getGroupCount();

  public native ControlGroup getGroup(int index);

  public native void updateSingleControlReference(ControlReference controlReference);

  public native void loadDefaultSettings();

  public native void clearSettings();

  public native void loadProfile(String path);

  public native void saveProfile(String path);

  public static native EmulatedController getGcPad(int controllerIndex);

  public static native EmulatedController getWiimote(int controllerIndex);

  public static native EmulatedController getWiimoteAttachment(int controllerIndex,
          int attachmentIndex);

  public static native int getSelectedWiimoteAttachment(int controllerIndex);

  public static native NumericSetting getSidewaysWiimoteSetting(int controllerIndex);
}
