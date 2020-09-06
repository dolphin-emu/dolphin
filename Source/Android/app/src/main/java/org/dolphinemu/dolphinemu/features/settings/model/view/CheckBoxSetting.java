package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class CheckBoxSetting extends SettingsItem
{
  protected boolean mDefaultValue;

  public CheckBoxSetting(String file, String section, String key, int titleId, int descriptionId,
          boolean defaultValue)
  {
    super(file, section, key, titleId, descriptionId);
    mDefaultValue = defaultValue;
  }

  public boolean isChecked(Settings settings)
  {
    return settings.getSection(getFile(), getSection()).getBoolean(getKey(), mDefaultValue);
  }

  public void setChecked(Settings settings, boolean checked)
  {
    settings.getSection(getFile(), getSection()).setBoolean(getKey(), checked);
  }

  @Override
  public int getType()
  {
    return TYPE_CHECKBOX;
  }
}
