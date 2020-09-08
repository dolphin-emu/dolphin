package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class InvertedCheckBoxSetting extends CheckBoxSetting
{
  public InvertedCheckBoxSetting(String file, String section, String key, int titleId,
          int descriptionId, boolean defaultValue)
  {
    super(file, section, key, titleId, descriptionId, !defaultValue);
  }

  @Override
  public boolean isChecked(Settings settings)
  {
    return !settings.getSection(getFile(), getSection()).getBoolean(getKey(), mDefaultValue);
  }

  @Override
  public void setChecked(Settings settings, boolean checked)
  {
    settings.getSection(getFile(), getSection()).setBoolean(getKey(), !checked);
  }
}
