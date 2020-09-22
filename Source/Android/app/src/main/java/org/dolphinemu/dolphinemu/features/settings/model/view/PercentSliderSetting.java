package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractFloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class PercentSliderSetting extends FloatSliderSetting
{
  public PercentSliderSetting(AbstractFloatSetting setting, int titleId, int descriptionId, int min,
          int max, String units)
  {
    super(setting, titleId, descriptionId, min, max, units);
  }

  @Override
  public int getSelectedValue(Settings settings)
  {
    return Math.round(mSetting.getFloat(settings) * 100);
  }

  @Override
  public void setSelectedValue(Settings settings, float selection)
  {
    mSetting.setFloat(settings, selection / 100);
  }

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
