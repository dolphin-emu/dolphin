package org.dolphinemu.dolphinemu.model.settings.view;


import org.dolphinemu.dolphinemu.model.settings.BooleanSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;

public class CheckBoxSetting extends SettingsItem
{
	public CheckBoxSetting(String key, Setting setting, int titleId, int descriptionId)
	{
		super(key, setting, titleId, descriptionId);
	}

	public boolean isChecked()
	{
		BooleanSetting setting = (BooleanSetting) getSetting();
		return setting.getValue();
	}

	public void setChecked(boolean checked)
	{
		BooleanSetting setting = (BooleanSetting) getSetting();
		setting.setValue(checked);
	}

	@Override
	public int getType()
	{
		return TYPE_CHECKBOX;
	}
}
