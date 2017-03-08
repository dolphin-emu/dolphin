package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.Setting;

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Each one corresponds to a {@link Setting} object, so this class's subclasses
 * should vaguely correspond to those subclasses. There are a few with multiple analogues
 * and a few with none (Headers, for example, do not correspond to anything in the ini
 * file.)
 */
public abstract class SettingsItem
{
	public static final int TYPE_HEADER = 0;
	public static final int TYPE_CHECKBOX = 1;
	public static final int TYPE_SINGLE_CHOICE = 2;
	public static final int TYPE_SLIDER = 3;
	public static final int TYPE_SUBMENU = 4;
	public static final int TYPE_INPUT_BINDING = 5;

	private String mKey;
	private String mSection;
	private int mFile;

	private Setting mSetting;

	private int mNameId;
	private int mDescriptionId;

	/**
	 * Base constructor. Takes a key / section name in case the third parameter, the Setting,
	 * is null; in which case, one can be constructed and saved using the key / section.
	 *
	 * @param key           Identifier for the Setting represented by this Item.
	 * @param section       Section to which the Setting belongs.
	 * @param setting       A possibly-null backing Setting, to be modified on UI events.
	 * @param nameId        Resource ID for a text string to be displayed as this setting's name.
	 * @param descriptionId Resource ID for a text string to be displayed as this setting's description.
	 */
	public SettingsItem(String key, String section, int file, Setting setting, int nameId, int descriptionId)
	{
		mKey = key;
		mSection = section;
		mFile = file;
		mSetting = setting;
		mNameId = nameId;
		mDescriptionId = descriptionId;
	}

	/**
	 *
	 * @return The identifier for the backing Setting.
	 */
	public String getKey()
	{
		return mKey;
	}

	/**
	 *
	 * @return The header under which the backing Setting belongs.
	 */
	public String getSection()
	{
		return mSection;
	}

	/**
	 *
	 * @return The file the backing Setting is saved to.
	 */
	public int getFile()
	{
		return mFile;
	}

	/**
	 *
	 * @return The backing Setting, possibly null.
	 */
	public Setting getSetting()
	{
		return mSetting;
	}

	/**
	 * Replace the backing setting with a new one. Generally used in cases where
	 * the backing setting is null.
	 *
	 * @param setting A non-null Setting.
	 */
	public void setSetting(Setting setting)
	{
		mSetting = setting;
	}

	/**
	 *
	 * @return A resource ID for a text string representing this Setting's name.
	 */
	public int getNameId()
	{
		return mNameId;
	}

	public int getDescriptionId()
	{
		return mDescriptionId;
	}

	/**
	 * Used by {@link org.dolphinemu.dolphinemu.ui.settings.SettingsAdapter}'s onCreateViewHolder()
	 * method to determine which type of ViewHolder should be created.
	 *
	 * @return An integer (ideally, one of the constants defined in this file)
	 */
	public abstract int getType();
}
