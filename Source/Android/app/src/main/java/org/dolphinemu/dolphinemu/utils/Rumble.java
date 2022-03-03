// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.SparseArray;
import android.view.InputDevice;

import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.settings.model.AdHocStringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

public class Rumble
{
  private static Vibrator phoneVibrator;
  private static final SparseArray<Vibrator> emuVibrators = new SparseArray<>();

  public static void initRumble(EmulationActivity activity)
  {
    clear();

    if (BooleanSetting.MAIN_PHONE_RUMBLE.getBooleanGlobal())
    {
      setPhoneVibrator(true, activity);
    }

    for (int i = 0; i < 8; i++)
    {
      String deviceName = AdHocStringSetting.getStringGlobal(Settings.FILE_DOLPHIN,
              Settings.SECTION_BINDINGS, SettingsFile.KEY_EMU_RUMBLE + i, "");

      if (!deviceName.isEmpty())
      {
        for (int id : InputDevice.getDeviceIds())
        {
          InputDevice device = InputDevice.getDevice(id);
          if (deviceName.equals(device.getDescriptor()))
          {
            Vibrator vib = device.getVibrator();
            if (vib != null && vib.hasVibrator())
              emuVibrators.put(i, vib);
          }
        }
      }
    }
  }

  public static void setPhoneVibrator(boolean set, EmulationActivity activity)
  {
    if (set)
    {
      Vibrator vib = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
      if (vib != null && vib.hasVibrator())
        phoneVibrator = vib;
    }
    else
    {
      phoneVibrator = null;
    }
  }

  private static void clear()
  {
    phoneVibrator = null;
    emuVibrators.clear();
  }

  public static void checkRumble(int padId, double state)
  {
    if (phoneVibrator != null)
      doRumble(phoneVibrator);

    if (emuVibrators.get(padId) != null)
      doRumble(emuVibrators.get(padId));
  }

  public static void doRumble(Vibrator vib)
  {
    // Check again that it exists and can vibrate
    if (vib != null && vib.hasVibrator())
    {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
      {
        vib.vibrate(VibrationEffect.createOneShot(100, VibrationEffect.DEFAULT_AMPLITUDE));
      }
      else
      {
        vib.vibrate(100);
      }
    }
  }
}
