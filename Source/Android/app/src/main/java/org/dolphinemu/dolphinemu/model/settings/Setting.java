package org.dolphinemu.dolphinemu.model.settings;

/**
 * Abstraction for a setting item as read from / written to Dolphin's configuration ini files.
 * These files generally consist of a key/value pair, though the type of value is ambiguous and
 * must be inferred at read-time. The type of value determines which child of this class is used
 * to represent the Setting.
 */
public abstract class Setting
{
	private String mKey;
	private String mSection;
	private int mFile;

	/**
	 * Base constructor.
	 *
	 * @param key     Everything to the left of the = in a line from the ini file.
	 * @param section The corresponding recent section header; e.g. [Core] or [Enhancements] without the brackets.
	 * @param file    The ini file the Setting is stored in.
	 */
	public Setting(String key, String section, int file)
	{
		mKey = key;
		mSection = section;
		mFile = file;
	}

	/**
	 *
	 * @return The identifier used to write this setting to the ini file.
	 */
	public String getKey()
	{
		return mKey;
	}

	/**
	 *
	 * @return The name of the header under which this Setting should be written in the ini file.
	 */
	public String getSection()
	{
		return mSection;
	}

	/**
	 *
	 * @return The ini file the Setting is stored in.
	 */
	public int getFile()
	{
		return mFile;
	}

	/**
	 * @return A representation of this Setting's backing value converted to a String (e.g. for serialization).
	 */
	public abstract String getValueAsString();
}
