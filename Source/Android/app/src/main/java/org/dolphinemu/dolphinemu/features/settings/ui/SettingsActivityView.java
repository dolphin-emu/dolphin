// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import android.os.Bundle;

import androidx.annotation.NonNull;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

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
   * End the activity.
   */
  void finish();

  /**
   * Called by a containing Fragment to tell the Activity that a setting was changed;
   * unless this has been called, the Activity will not save to disk.
   */
  void onSettingChanged();

  /**
   * Called by a containing Fragment to tell the containing Activity that the user wants to open the
   * MenuTag associated with a setting.
   *
   * @param menuTag The MenuTag of the setting.
   * @param value   The current value of the setting.
   */
  void onMenuTagAction(@NonNull MenuTag menuTag, int value);

  /**
   * Returns whether anything will happen when the user wants to open the MenuTag associated with a
   * setting, given the current value of the setting.
   *
   * @param menuTag The MenuTag of the setting.
   * @param value   The current value of the setting.
   */
  boolean hasMenuTagActionForValue(@NonNull MenuTag menuTag, int value);

  /**
   * Show loading dialog while loading the settings
   */
  void showLoading();

  /**
   * Hide the loading dialog
   */
  void hideLoading();

  /**
   * Tell the user that there is junk in the game INI and ask if they want to delete the whole file.
   */
  void showGameIniJunkDeletionQuestion();

  /**
   * Accesses the material toolbar layout and changes the title
   */
  void setToolbarTitle(String title);
}
