// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentActivity;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;

import java.util.ArrayList;

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
   * @param settings The (possibly null) result of the ini load operation.
   */
  void onSettingsFileLoaded(Settings settings);

  /**
   * Pass an ArrayList of settings to the View so that it can be displayed on screen.
   *
   * @param settingsList The settings to display
   */
  void showSettingsList(ArrayList<SettingsItem> settingsList);

  /**
   * Called by the containing Activity when an asynchronous load operation fails.
   * Instructs the Fragment to load the settings screen with defaults selected.
   */
  void loadDefaultSettings();

  /**
   * @return The Fragment's containing activity.
   */
  FragmentActivity getActivity();

  /**
   * @return The Fragment's SettingsAdapter.
   */
  SettingsAdapter getAdapter();

  /**
   * Tell the Fragment to tell the containing Activity to show a new
   * Fragment containing a submenu of settings.
   *
   * @param menuKey Identifier for the settings group that should be shown.
   */
  void loadSubMenu(MenuTag menuKey);

  /**
   * Tell the Fragment to tell the containing activity to display a toast message.
   *
   * @param message Text to be shown in the Toast
   */
  void showToastMessage(String message);

  /**
   * @return The backing settings store.
   */
  Settings getSettings();

  /**
   * Have the fragment tell the containing Activity that a setting was modified.
   */
  void onSettingChanged();

  /**
   * Have the fragment tell the containing Activity that the user wants to open the MenuTag
   * associated with a setting.
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
   * Sets whether the input mapping dialog should detect inputs from all devices,
   * not just the device configured for the controller.
   */
  void setMappingAllDevices(boolean allDevices);

  /**
   * Returns whether the input mapping dialog should detect inputs from all devices,
   * not just the device configured for the controller.
   */
  boolean isMappingAllDevices();

  /**
   * Shows or hides a warning telling the user that they're using incompatible controller settings.
   * The warning is hidden by default.
   *
   * @param visible Whether the warning should be visible.
   */
  void setOldControllerSettingsWarningVisibility(boolean visible);
}
