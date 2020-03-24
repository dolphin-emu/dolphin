package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.IntentFilter;
import android.os.Bundle;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.utils.DirectoryStateReceiver;

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
  void showSettingsFragment(MenuTag menuTag, Bundle extras, boolean addToStack, String gameId);

  /**
   * Called by a contained Fragment to get access to the Setting HashMap
   * loaded from disk, so that each Fragment doesn't need to perform its own
   * read operation.
   *
   * @return A possibly null HashMap of Settings.
   */
  Settings getSettings();

  /**
   * Used to provide the Activity with Settings HashMaps if a Fragment already
   * has one; for example, if a rotation occurs, the Fragment will not be killed,
   * but the Activity will, so the Activity needs to have its HashMaps resupplied.
   *
   * @param settings The ArrayList of all the Settings HashMaps.
   */
  void setSettings(Settings settings);

  /**
   * Called when an asynchronous load operation completes.
   *
   * @param settings The (possibly null) result of the ini load operation.
   */
  void onSettingsFileLoaded(Settings settings);

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
   * @param menuTag Identifier for the GCPad that was modified.
   * @param value   New setting for the GCPad.
   */
  void onGcPadSettingChanged(MenuTag menuTag, int value);

  /**
   * Called by a containing Fragment to tell the containing Activity that a Wiimote's setting
   * was modified.
   *
   * @param menuTag Identifier for Wiimote that was modified.
   * @param value   New setting for the Wiimote.
   */
  void onWiimoteSettingChanged(MenuTag menuTag, int value);

  /**
   * Called by a containing Fragment to tell the containing Activity that an extension setting
   * was modified.
   *
   * @param menuTag Identifier for the extension that was modified.
   * @param value   New setting for the extension.
   */
  void onExtensionSettingChanged(MenuTag menuTag, int value);

  /**
   * Show loading dialog while loading the settings
   */
  void showLoading();

  /**
   * Hide the loading the dialog
   */
  void hideLoading();

  /**
   * Show a hint to the user that the app needs write to external storage access
   */
  void showPermissionNeededHint();

  /**
   * Show a hint to the user that the app needs the external storage to be mounted
   */
  void showExternalStorageNotMountedHint();

  /**
   * Tell the user that there is junk in the game INI and ask if they want to delete the whole file.
   */
  void showGameIniJunkDeletionQuestion();

  /**
   * Start the DirectoryInitialization and listen for the result.
   *
   * @param receiver the broadcast receiver for the DirectoryInitialization
   * @param filter   the Intent broadcasts to be received.
   */
  void startDirectoryInitializationService(DirectoryStateReceiver receiver, IntentFilter filter);

  /**
   * Stop listening to the DirectoryInitialization.
   *
   * @param receiver The broadcast receiver to unregister.
   */
  void stopListeningToDirectoryInitializationService(DirectoryStateReceiver receiver);
}
