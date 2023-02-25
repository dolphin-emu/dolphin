// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;
import android.os.Vibrator;
import android.view.InputDevice;
import android.view.KeyEvent;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.utils.Rumble;

public class RumbleBindingSetting extends InputBindingSetting
{
  public RumbleBindingSetting(Context context, String file, String section, String key, int titleId,
          String gameId)
  {
    super(context, file, section, key, titleId, gameId);
  }

  /**
   * Just need the device when saving rumble.
   */
  @Override
  public void onKeyInput(Settings settings, KeyEvent keyEvent)
  {
    saveRumble(settings, keyEvent.getDevice());
  }

  /**
   * Just need the device when saving rumble.
   */
  @Override
  public void onMotionInput(Settings settings, InputDevice device,
          InputDevice.MotionRange motionRange, char axisDir)
  {
    saveRumble(settings, device);
  }

  private void saveRumble(Settings settings, InputDevice device)
  {
    Vibrator vibrator = device.getVibrator();
    if (vibrator != null && vibrator.hasVibrator())
    {
      setValue(settings, device.getDescriptor(), device.getName());
      Rumble.doRumble(vibrator);
    }
    else
    {
      setValue(settings, "",
              DolphinApplication.getAppContext().getString(R.string.rumble_not_found));
    }
  }

  @Override
  public int getType()
  {
    return TYPE_RUMBLE_BINDING;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return null;
  }
}
