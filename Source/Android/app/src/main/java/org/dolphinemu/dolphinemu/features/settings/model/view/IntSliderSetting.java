package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class IntSliderSetting extends SliderSetting
{
  private AbstractIntSetting mSetting;

  public IntSliderSetting(AbstractIntSetting setting, int titleId, int descriptionId, int max,
          String units)
  {
    super(titleId, descriptionId, max, units);
    mSetting = setting;
  }

  public int getSelectedValue(Settings settings)
  {
    return mSetting.getInt(settings);
  }

  public void setSelectedValue(Settings settings, int selection)
  {
    mSetting.setInt(settings, selection);
  }
}
