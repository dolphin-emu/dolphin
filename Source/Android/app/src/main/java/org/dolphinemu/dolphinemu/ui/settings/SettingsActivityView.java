package org.dolphinemu.dolphinemu.ui.settings;

import org.dolphinemu.dolphinemu.model.settings.SettingSection;

import java.util.HashMap;

public interface SettingsActivityView
{
	void showSettingsFragment(String menuTag, boolean addToStack);

	HashMap<String, SettingSection> getSettings();

	void setSettings(HashMap<String, SettingSection> settings);

	void onSettingsFileLoaded(HashMap<String, SettingSection> settings);

	void showToastMessage(String message);
}
