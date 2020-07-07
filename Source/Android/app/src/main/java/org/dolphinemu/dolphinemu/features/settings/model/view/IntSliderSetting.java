package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class IntSliderSetting extends SliderSetting
{
  private int mDefaultValue;

  public IntSliderSetting(String file, String section, String key, int titleId, int descriptionId,
          int max, String units, int defaultValue)
  {
    super(file, section, key, titleId, descriptionId, max, units);
    mDefaultValue = defaultValue;
  }

  public int getSelectedValue(Settings settings)
  {
    return settings.getSection(getFile(), getSection()).getInt(getKey(), mDefaultValue);
  }

  public void setSelectedValue(Settings settings, int selection)
  {
    settings.getSection(getFile(), getSection()).setInt(getKey(), selection);
  }
}
