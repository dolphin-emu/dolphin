package org.dolphinemu.dolphinemu.model.settings;

public abstract class Setting
{
	private String mKey;

	private SettingSection mSection;

	public Setting(String key, SettingSection section)
	{
		mKey = key;
		mSection = section;
	}

	public String getKey()
	{
		return mKey;
	}

	public SettingSection getSection()
	{
		return mSection;
	}

	public abstract String getValueAsString();
}
