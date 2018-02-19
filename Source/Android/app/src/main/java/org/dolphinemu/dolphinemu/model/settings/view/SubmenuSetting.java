package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.ui.settings.MenuTag;

public final class SubmenuSetting extends SettingsItem
{
	private MenuTag mMenuKey;

	public SubmenuSetting(String key, Setting setting, int titleId, int descriptionId, MenuTag menuKey)
	{
		super(key, null, 0, setting, titleId, descriptionId);
		mMenuKey = menuKey;
	}

	public MenuTag getMenuKey()
	{
		return mMenuKey;
	}

	@Override
	public int getType()
	{
		return TYPE_SUBMENU;
	}
}
