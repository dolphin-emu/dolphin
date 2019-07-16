package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SingleChoiceSettingDynamicDescriptions extends SettingsItem
{
  private int mDefaultValue;

  private int mChoicesId;
  private int mValuesId;
  private int mDescriptionChoicesId;
  private int mDescriptionValuesId;
  private MenuTag menuTag;

  public SingleChoiceSettingDynamicDescriptions(String key, String section, int titleId,
          int descriptionId,
          int choicesId, int valuesId, int descriptionChoicesId, int descriptionValuesId,
          int defaultValue, Setting setting, MenuTag menuTag)
  {
    super(key, section, setting, titleId, descriptionId);
    mValuesId = valuesId;
    mChoicesId = choicesId;
    mDescriptionChoicesId = descriptionChoicesId;
    mDescriptionValuesId = descriptionValuesId;
    mDefaultValue = defaultValue;
    this.menuTag = menuTag;
  }

  public SingleChoiceSettingDynamicDescriptions(String key, String section, int titleId,
          int descriptionId,
          int choicesId, int valuesId, int descriptionChoicesId, int descriptionValuesId,
          int defaultValue, Setting setting)
  {
    this(key, section, titleId, descriptionId, choicesId, valuesId, descriptionChoicesId,
            descriptionValuesId, defaultValue, setting, null);
  }

  public int getChoicesId()
  {
    return mChoicesId;
  }

  public int getValuesId()
  {
    return mValuesId;
  }

  public int getDescriptionChoicesId()
  {
    return mDescriptionChoicesId;
  }

  public int getDescriptionValuesId()
  {
    return mDescriptionValuesId;
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
    return TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS;
  }
}
