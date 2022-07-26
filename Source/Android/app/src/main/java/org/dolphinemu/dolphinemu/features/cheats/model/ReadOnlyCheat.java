// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public abstract class ReadOnlyCheat implements Cheat
{
  private Runnable mChangedCallback = null;

  public int trySet(@NonNull String name, @NonNull String creator, @NonNull String notes,
          @NonNull String code)
  {
    throw new UnsupportedOperationException();
  }

  public void setEnabled(boolean enabled)
  {
    setEnabledImpl(enabled);
    onChanged();
  }

  public void setChangedCallback(@Nullable Runnable callback)
  {
    mChangedCallback = callback;
  }

  protected void onChanged()
  {
    if (mChangedCallback != null)
      mChangedCallback.run();
  }

  protected abstract void setEnabledImpl(boolean enabled);
}
