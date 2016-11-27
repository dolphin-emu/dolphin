package org.dolphinemu.dolphinemu.model.settings.view;

import org.dolphinemu.dolphinemu.model.settings.BooleanSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.model.settings.StringSetting;

public final class InputBindingSetting extends SettingsItem
{
	public InputBindingSetting(String key, String section, int file, int titleId, Setting setting)
	{
		super(key, section, file, setting, titleId, 0);
	}

	public String getValue()
	{
		if (getSetting() == null)
		{
			return "";
		}

		StringSetting setting = (StringSetting) getSetting();
		return setting.getValue();
	}

	/**
	 * Write a value to the backing string. If that string was previously null,
	 * initializes a new one and returns it, so it can be added to the Hashmap.
	 *
	 * @param bind The input that will be bound
	 * @return null if overwritten successfully; otherwise, a newly created StringSetting.
	 */
	public StringSetting setValue(String bind)
	{
		if (getSetting() == null)
		{
			StringSetting setting = new StringSetting(getKey(), getSection(), getFile(), bind);
			setSetting(setting);
			return setting;
		}
		else
		{
			StringSetting setting = (StringSetting) getSetting();
			setting.setValue(bind);
			return null;
		}
	}

	@Override
	public int getType()
	{
		return TYPE_INPUT_BINDING;
	}
}
