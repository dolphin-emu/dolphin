package org.dolphinemu.dolphinemu.model.settings;

public final class IntSetting extends Setting
{
	private int mValue;

	public IntSetting(String key, String section, int value)
	{
		super(key, section);
		mValue = value;
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
}
