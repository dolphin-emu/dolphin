// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import android.os.Build;
import android.os.Vibrator;
import android.os.VibratorManager;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

@RequiresApi(api = Build.VERSION_CODES.S)
public final class DolphinVibratorManagerPassthrough implements DolphinVibratorManager
{
  private final VibratorManager mVibratorManager;

  public DolphinVibratorManagerPassthrough(@NonNull VibratorManager vibratorManager)
  {
    mVibratorManager = vibratorManager;
  }

  @Override @NonNull
  public Vibrator getVibrator(int vibratorId)
  {
    return mVibratorManager.getVibrator(vibratorId);
  }

  @Override @NonNull
  public int[] getVibratorIds()
  {
    return mVibratorManager.getVibratorIds();
  }
}
