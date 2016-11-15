package org.dolphinemu.dolphinemu.model.settings;

public final class BooleanSetting extends Setting
{
	private boolean mValue;

	public BooleanSetting(String key, String section, int file, boolean value)
	{
		super(key, section, file);
		mValue = value;
	}

	public boolean getValue()
	{
		return mValue;
	}

	public void setValue(boolean value)
	{
		mValue = value;
	}

	@Override
	public String getValueAsString()
	{
		return mValue ? "True" : "False";
	}
}
