// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public abstract class SliderSetting extends SettingsItem
{
  private int mMin;
  private int mMax;
  private String mUnits;
  private int mStepSize;

  public SliderSetting(Context context, int nameId, int descriptionId, int min, int max,
          String units, int stepSize)
  {
    super(context, nameId, descriptionId);
    mMin = min;
    mMax = max;
    mUnits = units;
    mStepSize = stepSize;
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

  public int getStepSize()
  {
    return mStepSize;
  }

  @Override
  public int getType()
  {
    return TYPE_SLIDER;
  }
}
