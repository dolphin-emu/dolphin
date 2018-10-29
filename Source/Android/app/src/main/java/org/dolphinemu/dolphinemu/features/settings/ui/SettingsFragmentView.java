package org.dolphinemu.dolphinemu.features.settings.ui;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

/**
 * Abstraction for a screen showing a list of settings. Instances of
 * this type of view will each display a layer of the setting hierarchy.
 */
public interface SettingsFragmentView
{
  /**
   * Pass an ArrayList to the View so that it can be displayed on screen.
   *
   * @param settings The result of converting the HashMap to an ArrayList
   */
  void showSettingsList(Settings settings);
}
