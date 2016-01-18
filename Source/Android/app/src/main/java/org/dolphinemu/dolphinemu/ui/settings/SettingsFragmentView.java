package org.dolphinemu.dolphinemu.ui.settings;

import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;

import java.util.ArrayList;
import java.util.HashMap;

public interface SettingsFragmentView
{
	void onSettingsFileLoaded(HashMap<String, SettingSection> settings);

	void passOptionsToActivity(HashMap<String, SettingSection> settings);

	void showSettingsList(ArrayList<SettingsItem> settingsList);
}
