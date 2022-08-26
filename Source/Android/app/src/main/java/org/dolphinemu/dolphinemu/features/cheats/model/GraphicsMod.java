// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class GraphicsMod extends ReadOnlyCheat
{
  @Keep
  private final long mPointer;

  // When a C++ GraphicsModGroup object is destroyed, it also destroys the GraphicsMods it owns.
  // To avoid getting dangling pointers, we keep a reference to the GraphicsModGroup here.
  @Keep
  private final GraphicsModGroup mParent;

  @Keep
  private GraphicsMod(long pointer, GraphicsModGroup parent)
  {
    mPointer = pointer;
    mParent = parent;
  }

  public boolean supportsCreator()
  {
    return true;
  }

  public boolean supportsNotes()
  {
    return true;
  }

  public boolean supportsCode()
  {
    return false;
  }

  @NonNull
  public native String getName();

  @NonNull
  public native String getCreator();

  @NonNull
  public native String getNotes();

  public boolean getUserDefined()
  {
    // Technically graphics mods can be user defined, but we don't support editing graphics mods
    // in the GUI, and editability is what this really controls
    return false;
  }

  public native boolean getEnabled();

  @Override
  protected native void setEnabledImpl(boolean enabled);
}
