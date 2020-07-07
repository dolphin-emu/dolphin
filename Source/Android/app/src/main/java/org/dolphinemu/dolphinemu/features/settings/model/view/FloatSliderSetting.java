package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class FloatSliderSetting extends SliderSetting
{
  protected float mDefaultValue;

  public FloatSliderSetting(String file, String section, String key, int titleId, int descriptionId,
          int max, String units, float defaultValue)
  {
    super(file, section, key, titleId, descriptionId, max, units);
    mDefaultValue = defaultValue;
  }

  public int getSelectedValue(Settings settings)
  {
    float value = settings.getSection(getFile(), getSection()).getFloat(getKey(), mDefaultValue);
    return Math.round(value);
  }

  public void setSelectedValue(Settings settings, float selection)
  {
    settings.getSection(getFile(), getSection()).setFloat(getKey(), selection);
  }
}
