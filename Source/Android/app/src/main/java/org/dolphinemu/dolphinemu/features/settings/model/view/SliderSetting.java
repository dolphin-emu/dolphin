package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public abstract class SliderSetting extends SettingsItem
{
  private int mMax;
  private String mUnits;

  public SliderSetting(String file, String section, String key, int nameId, int descriptionId,
          int max, String units)
  {
    super(file, section, key, nameId, descriptionId);
    mMax = max;
    mUnits = units;
  }

  public abstract int getSelectedValue(Settings settings);

  public int getMax()
  {
    return mMax;
  }

  public String getUnits()
  {
    return mUnits;
  }

  @Override
  public int getType()
  {
    return TYPE_SLIDER;
  }
}
