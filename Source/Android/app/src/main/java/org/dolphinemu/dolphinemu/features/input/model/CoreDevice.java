// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import androidx.annotation.Keep;

/**
 * Represents a C++ ciface::Core::Device.
 */
public final class CoreDevice
{
  /**
   * Represents a C++ ciface::Core::Device::Control.
   *
   * This class is non-static to ensure that the CoreDevice parent does not get garbage collected
   * while a Control is still accessible. (CoreDevice's finalizer may delete the native controls.)
   */
  @SuppressWarnings("InnerClassMayBeStatic")
  public final class Control
  {
    @Keep
    private final long mPointer;

    @Keep
    private Control(long pointer)
    {
      mPointer = pointer;
    }

    public native String getName();
  }

  @Keep
  private final long mPointer;

  @Keep
  private CoreDevice(long pointer)
  {
    mPointer = pointer;
  }

  @Override
  protected native void finalize();

  public native Control[] getInputs();

  public native Control[] getOutputs();
}
