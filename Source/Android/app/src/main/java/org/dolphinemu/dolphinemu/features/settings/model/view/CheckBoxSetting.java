package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class CheckBoxSetting extends SettingsItem
{
  protected AbstractBooleanSetting mSetting;

  public CheckBoxSetting(AbstractBooleanSetting setting, int titleId, int descriptionId)
  {
    super(titleId, descriptionId);
    mSetting = setting;
  }

  public boolean isChecked(Settings settings)
  {
    return mSetting.getBoolean(settings);
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

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
