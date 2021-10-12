// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public abstract class SliderSetting extends SettingsItem
{
  private int mMin;
  private int mMax;
  private String mUnits;

  public SliderSetting(Context context, int nameId, int descriptionId, int min, int max,
          String units)
  {
    super(context, nameId, descriptionId);
    mMin = min;
    mMax = max;
    mUnits = units;
  }

  public abstract int getSelectedValue(Settings settings);

  public int getMin()
  {
    return mMin;
  }

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
