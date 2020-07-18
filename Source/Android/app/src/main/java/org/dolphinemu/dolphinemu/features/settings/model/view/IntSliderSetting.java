package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class IntSliderSetting extends SliderSetting
{
  private IntSetting mSetting;
  private int mDefaultValue;

  public IntSliderSetting(IntSetting setting, int titleId, int descriptionId, int max,
          String units, int defaultValue)
  {
    super(titleId, descriptionId, max, units);
    mSetting = setting;
    mDefaultValue = defaultValue;
  }

  public int getSelectedValue(Settings settings)
  {
    return mSetting.getInt(settings, mDefaultValue);
  }

  public void setSelectedValue(Settings settings, int selection)
  {
    mSetting.setInt(settings, selection);
  }
}
