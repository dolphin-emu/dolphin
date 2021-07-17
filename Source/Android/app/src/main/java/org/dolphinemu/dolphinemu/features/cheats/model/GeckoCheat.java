// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

public class GeckoCheat implements Cheat
{
  @Keep
  private final long mPointer;

  @Keep
  private GeckoCheat(long pointer)
  {
    mPointer = pointer;
  }

  @Override
  public native void finalize();

  @NonNull
  public native String getName();

  @NonNull
  public static native GeckoCheat[] loadCodes(String gameId, int revision);
}
