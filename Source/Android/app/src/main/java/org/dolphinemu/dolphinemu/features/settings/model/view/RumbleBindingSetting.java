package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.os.Vibrator;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.utils.Rumble;

public class RumbleBindingSetting extends InputBindingSetting
{
  public RumbleBindingSetting(String file, String section, String key, int titleId)
  {
    super(file, section, key, titleId);
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
      Toast.makeText(DolphinApplication.getAppContext(), R.string.rumble_not_found,
              Toast.LENGTH_SHORT).show();
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
