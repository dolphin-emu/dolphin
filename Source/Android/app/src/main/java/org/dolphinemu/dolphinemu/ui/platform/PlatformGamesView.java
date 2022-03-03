// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform;

/**
 * Abstraction for a screen representing a single platform's games.
 */
public interface PlatformGamesView
{
  /**
   * Tell the view that a certain game's screenshot has been updated,
   * and should be redrawn on-screen.
   *
   * @param position The index of the game that should be redrawn.
   */
  void refreshScreenshotAtPosition(int position);

  /**
   * Pass a click event to the view's Presenter. Typically called from the
   * view's list adapter.
   *
   * @param gameId The ID of the game that was clicked.
   */
  void onItemClick(String gameId);

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
