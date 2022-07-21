// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

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
   * Called by a containing Fragment to tell the containing Activity that the Serial Port 1 setting
   * was modified.
   *
   * @param menuTag Identifier for the SerialPort that was modified.
   * @param value   New setting for the SerialPort.
   */
  void onSerialPort1SettingChanged(MenuTag menuTag, int value);

  /**
   * Have the fragment tell the containing Activity that a GCPad's setting was modified.
   *
   * @param menuTag Identifier for the GCPad that was modified.
   * @param value   New setting for the GCPad.
   */
  void onGcPadSettingChanged(MenuTag menuTag, int value);

  /**
   * Have the fragment tell the containing Activity that a Wiimote's setting was modified.
   *
   * @param menuTag Identifier for Wiimote that was modified.
   * @param value   New setting for the Wiimote.
   */
  void onWiimoteSettingChanged(MenuTag menuTag, int value);

  /**
   * Have the fragment tell the containing Activity that an extension setting was modified.
   *
   * @param menuTag Identifier for the extension that was modified.
   * @param value   New setting for the extension.
   */
  void onExtensionSettingChanged(MenuTag menuTag, int value);
}
