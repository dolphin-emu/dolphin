package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.FloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class PercentSliderSetting extends FloatSliderSetting
{
  public PercentSliderSetting(FloatSetting setting, int titleId, int descriptionId, int max,
          String units, float defaultValue)
  {
    super(setting, titleId, descriptionId, max, units, defaultValue / 100);
  }

  @Override
  public int getSelectedValue(Settings settings)
  {
    return Math.round(mSetting.getFloat(settings, mDefaultValue) * 100);
  }

  @Override
  public void setSelectedValue(Settings settings, float selection)
  {
    mSetting.setFloat(settings, selection / 100);
  }
}
