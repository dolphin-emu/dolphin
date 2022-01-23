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

  public native int getGroupCount();

  public native ControlGroup getGroup(int index);

  public native void updateSingleControlReference(ControlReference controlReference);

  public static native EmulatedController getGcPad(int controllerIndex);

  public static native EmulatedController getWiimote(int controllerIndex);

  public static native EmulatedController getWiimoteAttachment(int controllerIndex,
          int attachmentIndex);
}
