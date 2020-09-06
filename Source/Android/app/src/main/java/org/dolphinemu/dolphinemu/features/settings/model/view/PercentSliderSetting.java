package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class PercentSliderSetting extends FloatSliderSetting
{
  public PercentSliderSetting(String file, String section, String key, int titleId,
          int descriptionId, int max, String units, float defaultValue)
  {
    super(file, section, key, titleId, descriptionId, max, units, defaultValue / 100);
  }

  @Override
  public int getSelectedValue(Settings settings)
  {
    float value = settings.getSection(getFile(), getSection()).getFloat(getKey(), mDefaultValue);
    return Math.round(value * 100);
  }

  @Override
  public void setSelectedValue(Settings settings, float selection)
  {
    settings.getSection(getFile(), getSection()).setFloat(getKey(), selection / 100);
  }
}
