package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.FloatSetting;
import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.utils.Log;

public class SliderSetting extends SettingsItem
{
	private int mMax;

	public SliderSetting(String key, Setting setting, int titleId, int descriptionId, int max)
	{
		super(key, setting, titleId, descriptionId);
		mMax = max;
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
			return Math.round(floatSetting.getValue());
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

	@Override
	public int getType()
	{
		return TYPE_SLIDER;
	}
}
