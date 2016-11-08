package org.dolphinemu.dolphinemu.model.settings;

public final class StringSetting extends Setting
{
	private String mValue;

	public StringSetting(String key, String section, int file, String value)
	{
		super(key, section, file);
		mValue = value;
	}

	public String getValue()
	{
		return mValue;
	}

	public void setValue(String value)
	{
		mValue = value;
	}

	@Override
	public String getValueAsString()
	{
		return mValue;
	}
}
