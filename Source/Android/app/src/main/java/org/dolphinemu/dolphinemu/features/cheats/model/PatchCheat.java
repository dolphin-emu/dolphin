// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

public class PatchCheat extends AbstractCheat
{
  @Keep
  private final long mPointer;

  public PatchCheat()
  {
    mPointer = createNew();
  }

  @Keep
  private PatchCheat(long pointer)
  {
    mPointer = pointer;
  }

  @Override
  public native void finalize();

  private native long createNew();

  public boolean supportsCreator()
  {
    return false;
  }

  public boolean supportsNotes()
  {
    return false;
  }

  @NonNull
  public native String getName();

  @NonNull
  public native String getCode();

  public native boolean getUserDefined();

  public native boolean getEnabled();

  @Override
  protected native int trySetImpl(@NonNull String name, @NonNull String creator,
          @NonNull String notes, @NonNull String code);

  @Override
  protected native void setEnabledImpl(boolean enabled);

  @NonNull
  public static native PatchCheat[] loadCodes(String gameId, int revision);

  public static native void saveCodes(String gameId, int revision, PatchCheat[] codes);
}
