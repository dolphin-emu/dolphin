package org.dolphinemu.dolphinemu.model.settings;

public final class FloatSetting extends Setting
{
	private float mValue;

	public FloatSetting(String key, String section, int file, float value)
	{
		super(key, section, file);
		mValue = value;
	}

	public float getValue()
	{
		return mValue;
	}

	public void setValue(float value)
	{
		mValue = value;
	}

	@Override
	public String getValueAsString()
	{
		return Float.toString(mValue);
	}
}
