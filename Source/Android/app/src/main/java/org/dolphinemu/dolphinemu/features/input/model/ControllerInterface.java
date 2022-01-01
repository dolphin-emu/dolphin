// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import android.view.KeyEvent;
import android.view.MotionEvent;

/**
 * This class interfaces with the native ControllerInterface,
 * which is where the emulator core gets inputs from.
 */
public final class ControllerInterface
{
  /**
   * Activities which want to pass on inputs to native code
   * should call this in their own dispatchKeyEvent method.
   *
   * @return true if the emulator core seems to be interested in this event.
   * false if the event should be passed on to the default dispatchKeyEvent.
   */
  public static native boolean dispatchKeyEvent(KeyEvent event);

  /**
   * Activities which want to pass on inputs to native code
   * should call this in their own dispatchGenericMotionEvent method.
   *
   * @return true if the emulator core seems to be interested in this event.
   * false if the event should be passed on to the default dispatchGenericMotionEvent.
   */
  public static native boolean dispatchGenericMotionEvent(MotionEvent event);
}
