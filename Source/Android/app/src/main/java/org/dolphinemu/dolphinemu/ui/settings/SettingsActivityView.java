package org.dolphinemu.dolphinemu.ui.settings;

import org.dolphinemu.dolphinemu.model.settings.SettingSection;

import java.util.ArrayList;
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

	/**
	 * Called by a contained Fragment to get access to the Setting HashMap
	 * loaded from disk, so that each Fragment doesn't need to perform its own
	 * read operation.
	 *
	 * @param file The settings file to load.
	 * @return A possibly null HashMap of Settings.
	 */
	HashMap<String, SettingSection> getSettings(int file);

	/**
	 * Used to provide the Activity with Settings HashMaps if a Fragment already
	 * has one; for example, if a rotation occurs, the Fragment will not be killed,
	 * but the Activity will, so the Activity needs to have its HashMaps resupplied.
	 *
	 * @param settings The ArrayList of all the Settings HashMaps.
	 */
	void setSettings(ArrayList<HashMap<String, SettingSection>> settings);

	/**
	 * Called when an asynchronous load operation completes.
	 *
	 * @param settings The (possibly null) result of the ini load operation.
	 */
	void onSettingsFileLoaded(ArrayList<HashMap<String, SettingSection>> settings);

	/**
	 * Called when an asynchronous load operation fails.
	 */
	void onSettingsFileNotFound();

	/**
	 * Display a popup text message on screen.
	 *
	 * @param message The contents of the onscreen message.
	 */
	void showToastMessage(String message);

	/**
	 * Show the previous fragment.
	 */
	void popBackStack();

	/**
	 * End the activity.
	 */
	void finish();

	/**
	 * Called by a containing Fragment to tell the Activity that a setting was changed;
	 * unless this has been called, the Activity will not save to disk.
	 */
	void onSettingChanged();

	/**
	 * Called by a containing Fragment to tell the containing Activity that a GCPad's setting
	 * was modified.
	 *
	 * @param key   Identifier for the GCPad that was modified.
	 * @param value New setting for the GCPad.
	 */
	void onGcPadSettingChanged(String key, int value);

	/**
	 * Called by a containing Fragment to tell the containing Activity that a Wiimote's setting
	 * was modified.
	 *
	 * @param section Identifier for Wiimote that was modified; Wiimotes are identified by their section,
	 *                not their key.
	 * @param value   New setting for the Wiimote.
	 */
	void onWiimoteSettingChanged(String section, int value);

	/**
	 * Called by a containing Fragment to tell the containing Activity that an extension setting
	 * was modified.
	 *
	 * @param key   Identifier for the extension that was modified.
	 * @param value New setting for the extension.
	 */
	void onExtensionSettingChanged(String key, int value);
}
