package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.FloatSetting;
import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

public final class SliderSetting extends SettingsItem
{
	private int mMax;
	private int mDefaultValue;

	private String mUnits;

	public SliderSetting(String key, String section, int file, int titleId, int descriptionId, int max, String units, int defaultValue, Setting setting)
	{
		super(key, section, file, setting, titleId, descriptionId);
		mMax = max;
		mUnits = units;
		mDefaultValue = defaultValue;
	}

	public int getMax()
	{
		return mMax;
	}

	public int getSelectedValue()
	{
		Setting setting = getSetting();

		if (setting == null)
		{
			return mDefaultValue;
		}

		if (setting instanceof IntSetting)
		{
			IntSetting intSetting = (IntSetting) setting;
			return intSetting.getValue();
		}
		else if (setting instanceof FloatSetting)
		{
			FloatSetting floatSetting = (FloatSetting) setting;
			if (floatSetting.getKey().equals(SettingsFile.KEY_OVERCLOCK_PERCENT))
			{
				return Math.round(floatSetting.getValue() * 100);
			}
			else
			{
				return Math.round(floatSetting.getValue());
			}
		}
		else
		{
			Log.error("[SliderSetting] Error casting setting type.");
			return -1;
		}
	}

	/**
	 * Write a value to the backing int. If that int was previously null,
	 * initializes a new one and returns it, so it can be added to the Hashmap.
	 *
	 * @param selection New value of the int.
	 * @return null if overwritten successfully otherwise; a newly created IntSetting.
	 */
	public IntSetting setSelectedValue(int selection)
	{
		if (getSetting() == null)
		{
			IntSetting setting = new IntSetting(getKey(), getSection(), getFile(), selection);
			setSetting(setting);
			return setting;
		}
		else
		{
			IntSetting setting = (IntSetting) getSetting();
			setting.setValue(selection);
			return null;
		}
	}

	/**
	 * Write a value to the backing float. If that float was previously null,
	 * initializes a new one and returns it, so it can be added to the Hashmap.
	 *
	 * @param selection New value of the float.
	 * @return null if overwritten successfully otherwise; a newly created FloatSetting.
	 */
	public FloatSetting setSelectedValue(float selection)
	{
		if (getSetting() == null)
		{
			FloatSetting setting = new FloatSetting(getKey(), getSection(), getFile(), selection);
			setSetting(setting);
			return setting;
		}
		else
		{
			FloatSetting setting = (FloatSetting) getSetting();
			setting.setValue(selection);
			return null;
		}
	}

	public String getUnits()
	{
		return mUnits;
	}

	@Override
	public int getType()
	{
		return TYPE_SLIDER;
	}
}
