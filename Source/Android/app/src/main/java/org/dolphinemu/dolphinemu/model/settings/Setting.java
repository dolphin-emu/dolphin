package org.dolphinemu.dolphinemu.model.settings;

public abstract class Setting
{
	private String mKey;
	private String mSection;

	public Setting(String key, String section)
	{
		mKey = key;
		mSection = section;
	}

	public String getKey()
	{
		return mKey;
	}

	public String getSection()
	{
		return mSection;
	}

	public abstract String getValueAsString();
}
