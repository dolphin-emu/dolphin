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
   * To be called when the game file cache is updated.
   */
  void showGames();
}
