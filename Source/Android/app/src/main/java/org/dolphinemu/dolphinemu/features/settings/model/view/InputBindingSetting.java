package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;

public final class InputBindingSetting extends SettingsItem {
	public InputBindingSetting(String key, String section, int titleId, Setting setting) {
		super(key, section, setting, titleId, 0);
	}

	public String getValue() {
		if (getSetting() == null) {
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
	public StringSetting setValue(String bind) {
		if (getSetting() == null) {
			StringSetting setting = new StringSetting(getKey(), getSection(), bind);
			setSetting(setting);
			return setting;
		} else {
			StringSetting setting = (StringSetting) getSetting();
			setting.setValue(bind);
			return null;
		}
	}

	@Override
	public int getType() {
		return TYPE_INPUT_BINDING;
	}
}
