// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;
import android.content.SharedPreferences;
import android.view.InputDevice;
import android.view.KeyEvent;

import androidx.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class InputBindingSetting extends SettingsItem
{
  private String mFile;
  private String mSection;
  private String mKey;

  private String mGameId;

  public InputBindingSetting(Context context, String file, String section, String key, int titleId,
          String gameId)
  {
    super(context, titleId, 0);
    mFile = file;
    mSection = section;
    mKey = key;
    mGameId = gameId;
  }

  public String getKey()
  {
    return mKey;
  }

  public String getValue(Settings settings)
  {
    return settings.getSection(mFile, mSection).getString(mKey, "");
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
    editor.putString(mKey + mGameId, ui);
    editor.apply();

    settings.getSection(mFile, mSection).setString(mKey, bind);
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
    return mGameId;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return null;
  }
}
