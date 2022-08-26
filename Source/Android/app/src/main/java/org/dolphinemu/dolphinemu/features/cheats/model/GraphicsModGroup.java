package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

public class GraphicsModGroup
{
  @Keep
  private final long mPointer;

  @Keep
  private GraphicsModGroup(long pointer)
  {
    mPointer = pointer;
  }

  @Override
  public native void finalize();

  @NonNull
  public native GraphicsMod[] getMods();

  public native void save();

  @NonNull
  public static native GraphicsModGroup load(String gameId);
}
