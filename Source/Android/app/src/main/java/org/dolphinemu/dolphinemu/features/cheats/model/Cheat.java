// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public interface Cheat
{
  int TRY_SET_FAIL_NO_NAME = -1;
  int TRY_SET_SUCCESS = 0;

  @NonNull
  String getName();

  int trySet(@NonNull String name);

  boolean getUserDefined();

  boolean getEnabled();

  void setEnabled(boolean enabled);

  void setChangedCallback(@Nullable Runnable callback);
}
