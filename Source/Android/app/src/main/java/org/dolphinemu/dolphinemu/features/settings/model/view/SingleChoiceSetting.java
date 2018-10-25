package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SingleChoiceSetting extends SettingsItem
{
  private int mDefaultValue;

  private int mChoicesId;
  private int mValuesId;
  private MenuTag menuTag;

  public SingleChoiceSetting(String key, String section, int titleId, int descriptionId,
    int choicesId, int valuesId, int defaultValue, Setting setting, MenuTag menuTag)
  {
    super(key, section, setting, titleId, descriptionId);
    mValuesId = valuesId;
    mChoicesId = choicesId;
    mDefaultValue = defaultValue;
    this.menuTag = menuTag;
  }

  public SingleChoiceSetting(String key, String section, int titleId, int descriptionId,
    int choicesId, int valuesId, int defaultValue, Setting setting)
  {
    this(key, section, titleId, descriptionId, choicesId, valuesId, defaultValue, setting, null);
  }

  public int getChoicesId()
  {
    return mChoicesId;
  }

  public int getValuesId()
  {
    return mValuesId;
  }

  public int getSelectedValue()
  {
    if (getSetting() != null)
    {
      IntSetting setting = (IntSetting) getSetting();
      return setting.getValue();
    }
    else
    {
      return mDefaultValue;
    }
  }

  public MenuTag getMenuTag()
  {
    return menuTag;
  }

  /**
   * Write a value to the backing int. If that int was previously null,
   * initializes a new one and returns it, so it can be added to the Hashmap.
   *
   * @param selection New value of the int.
   * @return null if overwritten successfully otherwise; a newly created IntSetting.
   */
  public IntSetting setSelectedValue(int selection)
  {
    if (getSetting() == null)
    {
      IntSetting setting = new IntSetting(getKey(), getSection(), selection);
      setSetting(setting);
      return setting;
    }
    else
    {
      IntSetting setting = (IntSetting) getSetting();
      setting.setValue(selection);
      return null;
    }
  }

  @Override
  public int getType()
  {
    return TYPE_SINGLE_CHOICE;
  }
}
