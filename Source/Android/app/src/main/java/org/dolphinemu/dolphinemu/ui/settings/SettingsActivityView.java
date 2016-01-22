package org.dolphinemu.dolphinemu.ui.settings;

import org.dolphinemu.dolphinemu.model.settings.SettingSection;

import java.util.HashMap;

/**
 * Abstraction for the Activity that manages SettingsFragments.
 */
public interface SettingsActivityView
{
	/**
	 * Show a new SettingsFragment.
	 *
	 * @param menuTag    Identifier for the settings group that should be displayed.
	 * @param addToStack Whether or not this fragment should replace a previous one.
	 */
	void showSettingsFragment(String menuTag, boolean addToStack);

	HashMap<String, SettingSection> getSettings();

	void setSettings(HashMap<String, SettingSection> settings);

	/**
	 * Called when an asynchronous load operation completes.
	 *
	 * @param settings The (possibly null) result of the load operation.
	 */
	void onSettingsFileLoaded(HashMap<String, SettingSection> settings);

	void showToastMessage(String message);
}
