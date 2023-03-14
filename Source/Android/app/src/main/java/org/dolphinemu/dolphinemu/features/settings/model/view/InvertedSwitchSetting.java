// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class InvertedSwitchSetting extends SwitchSetting
{
  public InvertedSwitchSetting(Context context, AbstractBooleanSetting setting, int titleId,
          int descriptionId)
  {
    super(context, setting, titleId, descriptionId);
  }

  @Override
  public boolean isChecked()
  {
    return !mSetting.getBoolean();
  }

  @Override
  public void setChecked(Settings settings, boolean checked)
  {
    mSetting.setBoolean(settings, !checked);
  }

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
