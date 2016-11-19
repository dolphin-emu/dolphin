package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;

public final class SingleChoiceSetting extends SettingsItem
{
	private int mDefaultValue;

	private int mChoicesId;
	private int mValuesId;

	public SingleChoiceSetting(String key, String section, int file, int titleId, int descriptionId, int choicesId, int valuesId, int defaultValue, Setting setting)
	{
		super(key, section, file, setting, titleId, descriptionId);
		mValuesId = valuesId;
		mChoicesId = choicesId;
		mDefaultValue = defaultValue;
	}

	public int getChoicesId()
	{
		return mChoicesId;
	}

	public int getValuesId()
	{
		return mValuesId;
	}

	public int getSelectedValue()
	{
		if (getSetting() != null)
		{
			IntSetting setting = (IntSetting) getSetting();
			return setting.getValue();
		}
		else
		{
			return mDefaultValue;
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

	@Override
	public int getType()
	{
		return TYPE_SINGLE_CHOICE;
	}
}
