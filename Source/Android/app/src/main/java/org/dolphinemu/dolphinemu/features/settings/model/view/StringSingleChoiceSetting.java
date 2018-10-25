package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;

public class StringSingleChoiceSetting extends SettingsItem
{
  private String mDefaultValue;

  private String[] mChoicesId;
  private String[] mValuesId;

  public StringSingleChoiceSetting(String key, String section, int titleId, int descriptionId,
    String[] choicesId, String[] valuesId, String defaultValue, Setting setting)
  {
    super(key, section, setting, titleId, descriptionId);
    mValuesId = valuesId;
    mChoicesId = choicesId;
    mDefaultValue = defaultValue;
  }

  public String[] getChoicesId()
  {
    return mChoicesId;
  }

  public String[] getValuesId()
  {
    return mValuesId;
  }

  public String getValueAt(int index)
  {
    if (mValuesId == null)
      return null;

    if (index >= 0 && index < mValuesId.length)
    {
      return mValuesId[index];
    }

    return "";
  }

  public String getSelectedValue()
  {
    if (getSetting() != null)
    {
      StringSetting setting = (StringSetting) getSetting();
      return setting.getValue();
    }
    else
    {
      return mDefaultValue;
    }
  }

  public int getSelectValueIndex()
  {
    String selectedValue = getSelectedValue();
    for (int i = 0; i < mValuesId.length; i++)
    {
      if (mValuesId[i].equals(selectedValue))
      {
        return i;
      }
    }

    return -1;
  }

  /**
   * Write a value to the backing int. If that int was previously null,
   * initializes a new one and returns it, so it can be added to the Hashmap.
   *
   * @param selection New value of the int.
   * @return null if overwritten successfully otherwise; a newly created IntSetting.
   */
  public StringSetting setSelectedValue(String selection)
  {
    if (getSetting() == null)
    {
      StringSetting setting = new StringSetting(getKey(), getSection(), selection);
      setSetting(setting);
      return setting;
    }
    else
    {
      StringSetting setting = (StringSetting) getSetting();
      setting.setValue(selection);
      return null;
    }
  }

  @Override
  public int getType()
  {
    return TYPE_STRING_SINGLE_CHOICE;
  }
}


