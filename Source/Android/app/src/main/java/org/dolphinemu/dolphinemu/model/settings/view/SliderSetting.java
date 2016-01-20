package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.FloatSetting;
import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

public class SliderSetting extends SettingsItem
{
	private int mMax;

	private String mUnits;

	public SliderSetting(String key, Setting setting, int titleId, int descriptionId, int max, String units)
	{
		super(key, setting, titleId, descriptionId);
		mMax = max;
		mUnits = units;
	}

	public int getMax()
	{
		return mMax;
	}

	public int getSelectedValue()
	{
		Setting setting = getSetting();

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

	public void setSelectedValue(int selection)
	{
		IntSetting setting = (IntSetting) getSetting();
		setting.setValue(selection);
	}

	public void setSelectedValue(float selection)
	{
		FloatSetting setting = (FloatSetting) getSetting();
		setting.setValue(selection);
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
