package org.dolphinemu.dolphinemu.model.settings;

import java.util.HashMap;

/**
 * A semantically-related group of Settings objects. These Settings are
 * internally stored as a Hashmap.
 */
public final class SettingSection
{
	private String mName;

	private HashMap<String, Setting> mSettings = new HashMap<>();

	/**
	 * Create a new SettingSection with no Settings in it.
	 *
	 * @param name The header of this section; e.g. [Core] or [Enhancements] without the brackets.
	 */
	public SettingSection(String name)
	{
		mName = name;
	}

	public String getName()
	{
		return mName;
	}

	/**
	 * Convenience method; inserts a value directly into the backing Hashmap.
	 *
	 * @param key The key where the Setting will be inserted.
	 * @param setting The Setting to be inserted.
	 */
	public void putSetting(String key, Setting setting)
	{
		mSettings.put(key, setting);
	}

	/**
	 * Convenience method; gets a value directly from the backing Hashmap.
	 *
	 * @param key Used to retrieve the Setting.
	 * @return A Setting object (you should probably cast this before using)
	 */
	public Setting getSetting(String key)
	{
		return mSettings.get(key);
	}

	public HashMap<String, Setting> getSettings()
	{
		return mSettings;
	}
}
