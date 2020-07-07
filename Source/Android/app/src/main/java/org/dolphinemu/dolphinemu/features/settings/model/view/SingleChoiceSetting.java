package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SingleChoiceSetting extends SettingsItem
{
  private int mDefaultValue;

  private int mChoicesId;
  private int mValuesId;
  private MenuTag menuTag;

  public SingleChoiceSetting(String file, String section, String key, int titleId,
          int descriptionId, int choicesId, int valuesId, int defaultValue, MenuTag menuTag)
  {
    super(file, section, key, titleId, descriptionId);
    mValuesId = valuesId;
    mChoicesId = choicesId;
    mDefaultValue = defaultValue;
    this.menuTag = menuTag;
  }

  public SingleChoiceSetting(String file, String section, String key, int titleId,
          int descriptionId, int choicesId, int valuesId, int defaultValue)
  {
    this(file, section, key, titleId, descriptionId, choicesId, valuesId, defaultValue, null);
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
    return TYPE_SINGLE_CHOICE;
  }
}
