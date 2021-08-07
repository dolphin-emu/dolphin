// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public abstract class AbstractCheat implements Cheat
{
  private Runnable mChangedCallback = null;

  public int trySet(@NonNull String name)
  {
    if (name.isEmpty())
      return TRY_SET_FAIL_NO_NAME;

    int result = trySetImpl(name);

    if (result == TRY_SET_SUCCESS)
      onChanged();

    return result;
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

  protected abstract int trySetImpl(@NonNull String name);

  protected abstract void setEnabledImpl(boolean enabled);
}
