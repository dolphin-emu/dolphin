// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class SwitchSetting extends SettingsItem
{
  protected AbstractBooleanSetting mSetting;

  public SwitchSetting(Context context, AbstractBooleanSetting setting, int titleId,
          int descriptionId)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
  }

  public SwitchSetting(AbstractBooleanSetting setting, CharSequence title,
          CharSequence description)
  {
    super(title, description);
    mSetting = setting;
  }

  public boolean isChecked()
  {
    return mSetting.getBoolean();
  }

  public void setChecked(Settings settings, boolean checked)
  {
    mSetting.setBoolean(settings, checked);
  }

  @Override
  public int getType()
  {
    return TYPE_SWITCH;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
