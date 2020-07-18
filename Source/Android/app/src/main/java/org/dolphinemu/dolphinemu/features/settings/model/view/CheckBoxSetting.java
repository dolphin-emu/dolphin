package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class CheckBoxSetting extends SettingsItem
{
  protected BooleanSetting mSetting;
  protected boolean mDefaultValue;

  public CheckBoxSetting(BooleanSetting setting, int titleId, int descriptionId,
          boolean defaultValue)
  {
    super(titleId, descriptionId);
    mSetting = setting;
    mDefaultValue = defaultValue;
  }

  public boolean isChecked(Settings settings)
  {
    return mSetting.getBoolean(settings, mDefaultValue);
  }

  public void setChecked(Settings settings, boolean checked)
  {
    mSetting.setBoolean(settings, checked);
  }

  @Override
  public int getType()
  {
    return TYPE_CHECKBOX;
  }
}
