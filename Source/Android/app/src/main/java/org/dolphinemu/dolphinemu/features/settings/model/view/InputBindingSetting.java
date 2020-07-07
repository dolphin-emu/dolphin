package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.view.InputDevice;
import android.view.KeyEvent;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class InputBindingSetting extends SettingsItem
{
  private String gameId;

  public InputBindingSetting(String file, String section, String key, int titleId, String gameId)
  {
    super(file, section, key, titleId, 0);
    this.gameId = gameId;
  }

  public String getValue(Settings settings)
  {
    return settings.getSection(getFile(), getSection()).getString(getKey(), "");
  }

  /**
   * Saves the provided key input setting both to the INI file (so native code can use it) and as
   * an Android preference (so it persists correctly and is human-readable.)
   *
   * @param keyEvent KeyEvent of this key press.
   */
  public void onKeyInput(Settings settings, KeyEvent keyEvent)
  {
    InputDevice device = keyEvent.getDevice();
    String bindStr = "Device '" + device.getDescriptor() + "'-Button " + keyEvent.getKeyCode();
    String uiString = device.getName() + ": Button " + keyEvent.getKeyCode();
    setValue(settings, bindStr, uiString);
  }

  /**
   * Saves the provided motion input setting both to the INI file (so native code can use it) and as
   * an Android preference (so it persists correctly and is human-readable.)
   *
   * @param device      InputDevice from which the input event originated.
   * @param motionRange MotionRange of the movement
   * @param axisDir     Either '-' or '+'
   */
  public void onMotionInput(Settings settings, InputDevice device,
          InputDevice.MotionRange motionRange, char axisDir)
  {
    String bindStr =
            "Device '" + device.getDescriptor() + "'-Axis " + motionRange.getAxis() + axisDir;
    String uiString = device.getName() + ": Axis " + motionRange.getAxis() + axisDir;
    setValue(settings, bindStr, uiString);
  }

  public void setValue(Settings settings, String bind, String ui)
  {
    SharedPreferences
            preferences =
            PreferenceManager.getDefaultSharedPreferences(DolphinApplication.getAppContext());
    SharedPreferences.Editor editor = preferences.edit();
    editor.putString(getKey() + gameId, ui);
    editor.apply();

    settings.getSection(getFile(), getSection()).setString(getKey(), bind);
  }

  public void clearValue(Settings settings)
  {
    setValue(settings, "", "");
  }

  @Override
  public int getType()
  {
    return TYPE_INPUT_BINDING;
  }

  public String getGameId()
  {
    return gameId;
  }
}
