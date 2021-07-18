// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.Nullable;

public abstract class AbstractCheat implements Cheat
{
  private Runnable mChangedCallback = null;

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
