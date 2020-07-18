package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SingleChoiceSetting extends SettingsItem
{
  private IntSetting mSetting;
  private int mDefaultValue;

  private int mChoicesId;
  private int mValuesId;
  private MenuTag menuTag;

  public SingleChoiceSetting(IntSetting setting, int titleId, int descriptionId, int choicesId,
          int valuesId, int defaultValue, MenuTag menuTag)
  {
    super(titleId, descriptionId);
    mSetting = setting;
    mDefaultValue = defaultValue;
    mValuesId = valuesId;
    mChoicesId = choicesId;
    this.menuTag = menuTag;
  }

  public SingleChoiceSetting(IntSetting setting, int titleId, int descriptionId, int choicesId,
          int valuesId, int defaultValue)
  {
    this(setting, titleId, descriptionId, choicesId, valuesId, defaultValue, null);
  }

  public int getChoicesId()
  {
    return mChoicesId;
  }

  public int getValuesId()
  {
    return mValuesId;
  }

  public int getSelectedValue(Settings settings)
  {
    return mSetting.getInt(settings, mDefaultValue);
  }

  public MenuTag getMenuTag()
  {
    return menuTag;
  }

  public void setSelectedValue(Settings settings, int selection)
  {
    mSetting.setInt(settings, selection);
  }

  @Override
  public int getType()
  {
    return TYPE_SINGLE_CHOICE;
  }
}
