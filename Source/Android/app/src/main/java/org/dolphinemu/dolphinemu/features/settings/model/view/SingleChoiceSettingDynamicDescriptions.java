package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SingleChoiceSettingDynamicDescriptions extends SettingsItem
{
  private int mDefaultValue;

  private int mChoicesId;
  private int mValuesId;
  private int mDescriptionChoicesId;
  private int mDescriptionValuesId;
  private MenuTag menuTag;

  public SingleChoiceSettingDynamicDescriptions(String file, String section, String key,
          int titleId, int descriptionId, int choicesId, int valuesId, int descriptionChoicesId,
          int descriptionValuesId, int defaultValue, MenuTag menuTag)
  {
    super(file, section, key, titleId, descriptionId);
    mValuesId = valuesId;
    mChoicesId = choicesId;
    mDescriptionChoicesId = descriptionChoicesId;
    mDescriptionValuesId = descriptionValuesId;
    mDefaultValue = defaultValue;
    this.menuTag = menuTag;
  }

  public SingleChoiceSettingDynamicDescriptions(String file, String section, String key,
          int titleId, int descriptionId, int choicesId, int valuesId, int descriptionChoicesId,
          int descriptionValuesId, int defaultValue)
  {
    this(file, section, key, titleId, descriptionId, choicesId, valuesId, descriptionChoicesId,
            descriptionValuesId, defaultValue, null);
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

  public int getSelectedValue(Settings settings)
  {
    return settings.getSection(getFile(), getSection()).getInt(getKey(), mDefaultValue);
  }

  public MenuTag getMenuTag()
  {
    return menuTag;
  }

  public void setSelectedValue(Settings settings, int selection)
  {
    settings.getSection(getFile(), getSection()).setInt(getKey(), selection);
  }

  @Override
  public int getType()
  {
    return TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS;
  }
}
