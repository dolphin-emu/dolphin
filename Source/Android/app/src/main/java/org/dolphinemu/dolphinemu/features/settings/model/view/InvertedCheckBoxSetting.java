package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class InvertedCheckBoxSetting extends CheckBoxSetting
{
  public InvertedCheckBoxSetting(BooleanSetting setting, int titleId,
          int descriptionId, boolean defaultValue)
  {
    super(setting, titleId, descriptionId, !defaultValue);
  }

  @Override
  public boolean isChecked(Settings settings)
  {
    return !mSetting.getBoolean(settings, mDefaultValue);
  }

  @Override
  public void setChecked(Settings settings, boolean checked)
  {
    mSetting.setBoolean(settings, !checked);
  }
}
