// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform;

/**
 * Abstraction for a screen representing a single platform's games.
 */
public interface PlatformGamesView
{
  /**
   * Shows or hides the loading indicator.
   */
  void setRefreshing(boolean refreshing);

  /**
   * To be called when the game file cache is updated.
   */
  void showGames();

  /**
   * Re-fetches game metadata from the game file cache.
   */
  void refetchMetadata();
}
