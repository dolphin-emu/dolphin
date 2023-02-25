// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;

public class CheatItem
{
  public static final int TYPE_CHEAT = 0;
  public static final int TYPE_HEADER = 1;
  public static final int TYPE_ACTION = 2;

  private final @Nullable Cheat mCheat;
  private final int mString;
  private final int mType;

  public CheatItem(@NonNull Cheat cheat)
  {
    mCheat = cheat;
    mString = 0;
    mType = TYPE_CHEAT;
  }

  public CheatItem(int type, int string)
  {
    mCheat = null;
    mString = string;
    mType = type;
  }

  @Nullable
  public Cheat getCheat()
  {
    return mCheat;
  }

  public int getString()
  {
    return mString;
  }

  public int getType()
  {
    return mType;
  }
}
