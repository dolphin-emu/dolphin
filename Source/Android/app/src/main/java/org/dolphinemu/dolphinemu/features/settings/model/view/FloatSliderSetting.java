// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractFloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class FloatSliderSetting extends SliderSetting
{
  protected AbstractFloatSetting mSetting;

  public FloatSliderSetting(Context context, AbstractFloatSetting setting, int titleId,
          int descriptionId, int min, int max, String units, int stepSize)
  {
    super(context, titleId, descriptionId, min, max, units, stepSize);
    mSetting = setting;
  }

  public FloatSliderSetting(AbstractFloatSetting setting, CharSequence name,
          CharSequence description, int min, int max, String units)
  {
    super(name, description, min, max, units);
    mSetting = setting;
  }

  public int getSelectedValue()
  {
    return Math.round(mSetting.getFloat());
  }

  public void setSelectedValue(Settings settings, float selection)
  {
    mSetting.setFloat(settings, selection);
  }

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
