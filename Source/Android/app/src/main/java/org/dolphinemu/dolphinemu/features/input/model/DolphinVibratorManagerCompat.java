// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import android.os.Vibrator;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public final class DolphinVibratorManagerCompat implements DolphinVibratorManager
{
  private final Vibrator mVibrator;
  private final int[] mIds;

  public DolphinVibratorManagerCompat(@Nullable Vibrator vibrator)
  {
    mVibrator = vibrator;
    mIds = vibrator != null && vibrator.hasVibrator() ? new int[]{0} : new int[]{};
  }

  @Override @NonNull
  public Vibrator getVibrator(int vibratorId)
  {
    if (vibratorId > mIds.length)
      throw new IndexOutOfBoundsException();

    return mVibrator;
  }

  @Override @NonNull
  public int[] getVibratorIds()
  {
    return mIds;
  }
}
