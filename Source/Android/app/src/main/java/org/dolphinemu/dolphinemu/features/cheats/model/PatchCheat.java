// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

public class PatchCheat extends AbstractCheat
{
  @Keep
  private final long mPointer;

  @Keep
  private PatchCheat(long pointer)
  {
    mPointer = pointer;
  }

  @Override
  public native void finalize();

  @NonNull
  public native String getName();

  public native boolean getUserDefined();

  public native boolean getEnabled();

  @Override
  protected native int trySetImpl(@NonNull String name);

  @Override
  protected native void setEnabledImpl(boolean enabled);

  @NonNull
  public static native PatchCheat[] loadCodes(String gameId, int revision);

  public static native void saveCodes(String gameId, int revision, PatchCheat[] codes);
}
