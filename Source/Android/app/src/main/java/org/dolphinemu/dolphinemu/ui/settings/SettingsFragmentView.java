package org.dolphinemu.dolphinemu.ui.settings;

import android.app.Activity;

import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;

import java.util.ArrayList;
import java.util.HashMap;

/**
 * Abstraction for a screen showing a list of settings. Instances of
 * this type of view will each display a layer of the setting hierarchy.
 */
public interface SettingsFragmentView
{
	/**
	 * Called by the containing Activity to notify the Fragment that an
	 * asynchronous load operation completed.
	 *
	 * @param settings The potentially-null result of the load operation.
	 */
	void onSettingsFileLoaded(HashMap<String, SettingSection> settings);

	/**
	 * Pass a settings Hashmap to the containing activity, so that it can
	 * share the Hashmap with other SettingsFragments; useful so that rotations
	 * do not require an additional load operation.
	 *
	 * @param settings A Hashmap containing all the settings
	 */
	void passSettingsToActivity(HashMap<String, SettingSection> settings);

	/**
	 * Pass an ArrayList to the View so that it can be displayed on screen.
	 *
	 * @param settingsList The result of converting the Hashmap to an ArrayList
	 */
	void showSettingsList(ArrayList<SettingsItem> settingsList);

	/**
	 * @return The Fragment's containing activity.
	 */
	Activity getActivity();

	/**
	 * Tell the Fragment to tell the containing Activity to show a new
	 * Fragment containing a submenu of settings.
	 *
	 * @param menuKey Identifier for the settings group that should be shown.
	 */
	void loadSubMenu(String menuKey);

	/**
	 * Tell the Fragment to tell the containing activity to display a toast message.
	 *
	 * @param message Text to be shown in the Toast
	 */
	void showToastMessage(String message);

	/**
	 * Have the fragment add a setting to the Hashmap. Useful if the setting
	 * was missing from the .ini file, but included in the UI.
	 *
	 * @param setting The (possibly missing) new setting.
	 */
	void addSetting(Setting setting);
}
