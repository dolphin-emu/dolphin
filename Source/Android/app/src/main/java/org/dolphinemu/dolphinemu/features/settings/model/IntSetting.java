package org.dolphinemu.dolphinemu.features.settings.model;

import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class IntSetting extends Setting
{
	private int mValue;
	private MenuTag menuTag;

	public IntSetting(String key, String section, int file, int value)
	{
		super(key, section, file);
		mValue = value;
	}

	public IntSetting(String key, String section, int file, int value, MenuTag menuTag)
	{
		super(key, section, file);
		mValue = value;
		this.menuTag = menuTag;
	}

	public int getValue()
	{
		return mValue;
	}

	public void setValue(int value)
	{
		mValue = value;
	}

	@Override
	public String getValueAsString()
	{
		return Integer.toString(mValue);
	}

	public MenuTag getMenuTag()
	{
		return menuTag;
	}

}
