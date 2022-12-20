// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main;

import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

/**
 * Abstraction for the screen that shows on application launch.
 * Implementations will differ primarily to target touch-screen
 * or non-touch screen devices.
 */
public interface MainView
{
  /**
   * Pass the view the native library's version string. Displaying
   * it is optional.
   *
   * @param version A string pulled from native code.
   */
  void setVersionString(String version);

  void launchSettingsActivity(MenuTag menuTag);

  void launchFileListActivity();

  void launchOpenFileActivity(int requestCode);

  /**
   * Shows or hides the loading indicator.
   */
  void setRefreshing(boolean refreshing);

  /**
   * To be called when the game file cache is updated.
   */
  void showGames();

  void reloadGrid();

  void showGridOptions();
}
