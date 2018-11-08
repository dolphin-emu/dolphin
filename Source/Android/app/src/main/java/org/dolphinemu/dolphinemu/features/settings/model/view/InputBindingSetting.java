package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.view.InputDevice;
import android.view.KeyEvent;

import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;

public class InputBindingSetting extends SettingsItem
{
  public InputBindingSetting(String key, String section, int titleId, Setting setting)
  {
    super(key, section, setting, titleId, 0);
  }

  public String getValue()
  {
    if (getSetting() == null)
    {
      return "";
    }

    StringSetting setting = (StringSetting) getSetting();
    return setting.getValue();
  }

  /**
   * Saves the provided key input setting both to the INI file (so native code can use it) and as
   * an Android preference (so it persists correctly and is human-readable.)
   *
   * @param keyEvent KeyEvent of this key press.
   */
  public void onKeyInput(KeyEvent keyEvent)
  {
    InputDevice device = keyEvent.getDevice();
    String bindStr = "Device '" + device.getDescriptor() + "'-Button " + keyEvent.getKeyCode();
    setValue(bindStr);
  }

  /**
   * Saves the provided motion input setting both to the INI file (so native code can use it) and as
   * an Android preference (so it persists correctly and is human-readable.)
   *
   * @param device      InputDevice from which the input event originated.
   * @param motionRange MotionRange of the movement
   * @param axisDir     Either '-' or '+'
   */
  public void onMotionInput(InputDevice device, InputDevice.MotionRange motionRange, char axisDir)
  {
    String bindStr =
            "Device '" + device.getDescriptor() + "'-Axis " + motionRange.getAxis() + axisDir;
    setValue(bindStr);
  }

  /**
   * Write a value to the backing string. If that string was previously null,
   * initializes a new one and returns it, so it can be added to the Hashmap.
   *
   * @param bind The input that will be bound
   */
  public void setValue(String bind)
  {
    if (getSetting() == null)
    {
      StringSetting setting = new StringSetting(getKey(), getSection(), bind);
      setSetting(setting);
    }
    else
    {
      StringSetting setting = (StringSetting) getSetting();
      setting.setValue(bind);
    }
  }

  public void clearValue()
  {
    setValue("");
  }

  @Override
  public int getType()
  {
    return TYPE_INPUT_BINDING;
  }

  public String getSettingText()
  {
    String uiText = null;
    if(getSetting() != null)
    {
      String bindStr = getSetting().getValueAsString();
      int index = bindStr.indexOf('-');
      if(index > 0)
      {
        uiText = bindStr.substring(index + 1);
      }
      else
      {
        uiText = bindStr;
      }
    }
    return uiText;
  }
}
