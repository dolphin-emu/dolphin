package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

public final class FloatSliderSetting extends SliderSetting
{
  private float mDefaultValue;

  public FloatSliderSetting(String file, String section, String key, int titleId, int descriptionId,
          int max, String units, float defaultValue)
  {
    super(file, section, key, titleId, descriptionId, max, units);
    mDefaultValue = defaultValue;

    if (isPercentSetting())
      mDefaultValue /= 100;
  }

  public int getSelectedValue(Settings settings)
  {
    float value = settings.getSection(getFile(), getSection()).getFloat(getKey(), mDefaultValue);
    return Math.round(isPercentSetting() ? value * 100 : value);
  }

  public void setSelectedValue(Settings settings, float selection)
  {
    if (isPercentSetting())
      selection /= 100;

    settings.getSection(getFile(), getSection()).setFloat(getKey(), selection);
  }

  private boolean isPercentSetting()
  {
    return getKey().equals(SettingsFile.KEY_OVERCLOCK_PERCENT)
            || getKey().equals(SettingsFile.KEY_SPEED_LIMIT);
  }
}
