// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform

/**
 * Abstraction for a screen representing a single platform's games.
 */
interface PlatformGamesView {
    /**
     * Pass a click event to the view's Presenter. Typically called from the
     * view's list adapter.
     *
     * @param gameId The ID of the game that was clicked.
     */
    fun onItemClick(gameId: String) { /* Empty default impl */ }

    /**
     * Shows or hides the loading indicator.
     */
    fun setRefreshing(refreshing: Boolean)

    /**
     * To be called when the game file cache is updated.
     */
    fun showGames()

    /**
     * Re-fetches game metadata from the game file cache.
     */
    fun refetchMetadata()
}
