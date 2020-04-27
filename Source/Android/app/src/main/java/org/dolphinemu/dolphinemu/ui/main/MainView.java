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

  void launchOpenFileActivity();

  void launchInstallWAD();

  /**
   * To be called when the game file cache is updated.
   */
  void showGames();
}
