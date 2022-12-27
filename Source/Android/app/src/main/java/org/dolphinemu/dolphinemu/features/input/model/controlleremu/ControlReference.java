// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;

/**
 * Represents a C++ ControlReference.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
public class ControlReference
{
  @Keep
  private final long mPointer;

  @Keep
  private ControlReference(long pointer)
  {
    mPointer = pointer;
  }

  public native double getState();

  public native String getExpression();

  /**
   * Sets the expression for this control reference.
   *
   * @param expr The new expression
   * @return null on success, a human-readable error on failure
   */
  @Nullable
  public native String setExpression(String expr);

  public native boolean isInput();
}
