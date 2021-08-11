// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class GeckoCheat extends AbstractCheat
{
  @Keep
  private final long mPointer;

  public GeckoCheat()
  {
    mPointer = createNew();
  }

  @Keep
  private GeckoCheat(long pointer)
  {
    mPointer = pointer;
  }

  @Override
  public native void finalize();

  private native long createNew();

  @Override
  public boolean equals(@Nullable Object obj)
  {
    return obj != null && getClass() == obj.getClass() && equalsImpl((GeckoCheat) obj);
  }

  public boolean supportsCreator()
  {
    return true;
  }

  public boolean supportsNotes()
  {
    return true;
  }

  @NonNull
  public native String getName();

  @NonNull
  public native String getCreator();

  @NonNull
  public native String getNotes();

  @NonNull
  public native String getCode();

  public native boolean getUserDefined();

  public native boolean getEnabled();

  public native boolean equalsImpl(@NonNull GeckoCheat other);

  @Override
  protected native int trySetImpl(@NonNull String name, @NonNull String creator,
          @NonNull String notes, @NonNull String code);

  @Override
  protected native void setEnabledImpl(boolean enabled);

  @NonNull
  public static native GeckoCheat[] loadCodes(String gameId, int revision);

  public static native void saveCodes(String gameId, int revision, GeckoCheat[] codes);

  @Nullable
  public static native GeckoCheat[] downloadCodes(String gameTdbId);
}
