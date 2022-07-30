// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public interface Cheat
{
  int TRY_SET_FAIL_CODE_MIXED_ENCRYPTION = -3;
  int TRY_SET_FAIL_NO_CODE_LINES = -2;
  int TRY_SET_FAIL_NO_NAME = -1;
  int TRY_SET_SUCCESS = 0;
  // Result codes greater than 0 represent an error on the corresponding code line (one-indexed)

  boolean supportsCreator();

  boolean supportsNotes();

  boolean supportsCode();

  @NonNull
  String getName();

  @NonNull
  default String getCreator()
  {
    return "";
  }

  @NonNull
  default String getNotes()
  {
    return "";
  }

  @NonNull
  default String getCode()
  {
    return "";
  }

  int trySet(@NonNull String name, @NonNull String creator, @NonNull String notes,
          @NonNull String code);

  boolean getUserDefined();

  boolean getEnabled();

  void setEnabled(boolean enabled);

  void setChangedCallback(@Nullable Runnable callback);
}
