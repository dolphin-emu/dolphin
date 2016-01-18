package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;

public class SingleChoiceSetting extends SettingsItem
{
	private int mChoicesId;
	private int mValuesId;

	public SingleChoiceSetting(String key, Setting setting, int titleId, int descriptionId, int choicesId, int valuesId)
	{
		super(key, setting, titleId, descriptionId);
		mChoicesId = choicesId;
		mValuesId = valuesId;
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
		IntSetting setting = (IntSetting) getSetting();
		return setting.getValue();
	}

	public void setSelectedValue(int selection)
	{
		IntSetting setting = (IntSetting) getSetting();
		setting.setValue(selection);
	}

	@Override
	public int getType()
	{
		return TYPE_SINGLE_CHOICE;
	}
}
