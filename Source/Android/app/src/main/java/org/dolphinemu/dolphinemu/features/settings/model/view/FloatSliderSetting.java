package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.FloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class FloatSliderSetting extends SliderSetting
{
  protected FloatSetting mSetting;
  protected float mDefaultValue;

  public FloatSliderSetting(FloatSetting setting, int titleId, int descriptionId, int max,
          String units, float defaultValue)
  {
    super(titleId, descriptionId, max, units);
    mSetting = setting;
    mDefaultValue = defaultValue;
  }

  public int getSelectedValue(Settings settings)
  {
    return Math.round(mSetting.getFloat(settings, mDefaultValue));
  }

  public void setSelectedValue(Settings settings, float selection)
  {
    mSetting.setFloat(settings, selection);
  }
}
