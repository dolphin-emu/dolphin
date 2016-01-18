package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.Setting;

public abstract class SettingsItem
{
	public static final int TYPE_HEADER = 0;
	public static final int TYPE_CHECKBOX = 1;
	public static final int TYPE_SINGLE_CHOICE = 2;
	public static final int TYPE_SLIDER = 3;
	public static final int TYPE_SUBMENU = 4;

	private String mKey;
	private Setting mSetting;

	private int mTitleId;
	private int mDescriptionId;

	public SettingsItem(String key, Setting setting, int titleId, int descriptionId)
	{
		mKey = key;
		mSetting = setting;
		mTitleId = titleId;
		mDescriptionId = descriptionId;
	}

	public String getKey()
	{
		return mKey;
	}

	public Setting getSetting()
	{
		return mSetting;
	}

	public int getNameId()
	{
		return mTitleId;
	}

	public int getDescriptionId()
	{
		return mDescriptionId;
	}

	public abstract int getType();
}
