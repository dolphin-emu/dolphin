package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.view.InputDevice;
import android.view.KeyEvent;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AdHocStringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class InputBindingSetting extends SettingsItem
{
  private final String mFile;
  private final String mSection;
  private final String mKey;

  public InputBindingSetting(String file, String section, String key, int titleId)
  {
    super(titleId, 0);
    mFile = file;
    mSection = section;
    mKey = key;
  }

  public String getKey()
  {
    return mKey;
  }

  public String getValue(Settings settings)
  {
    return settings.getSection(mFile, mSection).getString(mKey, "");
  }

  public String getDescriptionValue()
  {
    return AdHocStringSetting.getStringGlobal(mFile, mSection, mKey + "Description", "");
  }

  /**
   * Saves the provided input setting as both native code and user-readable keys since there is no
   * easy way to convert between the two.
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
   * Saves the provided input setting as both native code and user-readable keys since there is no
   * easy way to convert between the two.
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
    new AdHocStringSetting(mFile, mSection, mKey, "").setString(settings, bind);
    new AdHocStringSetting(mFile, mSection, mKey + "Description", "").setString(settings, ui);
  }

  public void clearValue(Settings settings)
  {
    new AdHocStringSetting(mFile, mSection, mKey, "").delete(settings);
    new AdHocStringSetting(mFile, mSection, mKey + "Description", "").delete(settings);
  }

  @Override
  public int getType()
  {
    return TYPE_INPUT_BINDING;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return null;
  }
}
