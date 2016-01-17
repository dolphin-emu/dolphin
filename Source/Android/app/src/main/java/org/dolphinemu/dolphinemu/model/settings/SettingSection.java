package org.dolphinemu.dolphinemu.model.settings;

import java.util.HashMap;

public class SettingSection
{
	private String mName;

	private HashMap<String, Setting> mSettings = new HashMap<>();

	public SettingSection(String name)
	{
		mName = name;
	}

	public String getName()
	{
		return mName;
	}

	public void putSetting(String key, Setting setting)
	{
		mSettings.put(key, setting);
	}

	public Setting getSetting(String key)
	{
		return mSettings.get(key);
	}

	public HashMap<String, Setting> getSettings()
	{
		return mSettings;
	}
}
