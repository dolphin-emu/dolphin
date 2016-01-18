package org.dolphinemu.dolphinemu.model.settings.view;


import org.dolphinemu.dolphinemu.model.settings.Setting;

public class HeaderSetting extends SettingsItem
{
	public HeaderSetting(String key, Setting setting, int titleId, int descriptionId)
	{
		super(key, setting, titleId, descriptionId);
	}

	@Override
	public int getType()
	{
		return SettingsItem.TYPE_HEADER;
	}
}
