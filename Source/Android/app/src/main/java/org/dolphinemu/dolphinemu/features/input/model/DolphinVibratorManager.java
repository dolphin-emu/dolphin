// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import android.os.Vibrator;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

/**
 * A wrapper around {@link android.os.VibratorManager}, for backwards compatibility.
 */
public interface DolphinVibratorManager
{
  @Keep @NonNull
  Vibrator getVibrator(int vibratorId);

  @Keep @NonNull
  int[] getVibratorIds();
}
