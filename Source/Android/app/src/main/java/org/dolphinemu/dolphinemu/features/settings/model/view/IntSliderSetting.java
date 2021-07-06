// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class IntSliderSetting extends SliderSetting
{
  private AbstractIntSetting mSetting;

  public IntSliderSetting(Context context, AbstractIntSetting setting, int titleId,
          int descriptionId, int min, int max, String units)
  {
    super(context, titleId, descriptionId, min, max, units);
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

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
