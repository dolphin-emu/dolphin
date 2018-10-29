package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.SparseArray;
import android.view.InputDevice;

import org.dolphinemu.dolphinemu.features.settings.model.SettingSection;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

public class Rumble
{
  private static Vibrator phoneVibrator;
  private static SparseArray<Vibrator> emuVibrators;

  public static void initDeviceRumble()
  {
    Settings settings = new Settings();
    settings.loadSettings(null);
    SettingSection section = settings.getSection(Settings.SECTION_BINDINGS);

    emuVibrators = new SparseArray<>();
    for (int i = 0; i < 8; i++)
    {
      StringSetting deviceName =
              (StringSetting) section.getSetting(SettingsFile.KEY_EMU_RUMBLE + i);
      if (deviceName != null && !deviceName.getValue().isEmpty())
      {
        for (int id : InputDevice.getDeviceIds())
        {
          InputDevice device = InputDevice.getDevice(id);
          if (deviceName.getValue().equals(device.getDescriptor()))
          {
            Vibrator vib = device.getVibrator();
            if (vib != null && vib.hasVibrator())
              emuVibrators.put(i, vib);
          }
        }
      }
    }
  }

  public static void setPhoneRumble(Activity activity, boolean enable)
  {
    if (enable)
    {
      if(phoneVibrator != null)
        return;

      Vibrator vib = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
      if (vib != null && vib.hasVibrator())
        phoneVibrator = vib;
    }
    else
    {
      phoneVibrator = null;
    }
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
