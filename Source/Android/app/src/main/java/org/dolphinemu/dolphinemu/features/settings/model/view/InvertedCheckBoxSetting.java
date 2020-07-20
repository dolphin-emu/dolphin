package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class InvertedCheckBoxSetting extends CheckBoxSetting
{
  public InvertedCheckBoxSetting(AbstractBooleanSetting setting, int titleId,
          int descriptionId)
  {
    super(setting, titleId, descriptionId);
  }

  @Override
  public boolean isChecked(Settings settings)
  {
    return !mSetting.getBoolean(settings);
  }

  @Override
  public void setChecked(Settings settings, boolean checked)
  {
    mSetting.setBoolean(settings, !checked);
  }
}
