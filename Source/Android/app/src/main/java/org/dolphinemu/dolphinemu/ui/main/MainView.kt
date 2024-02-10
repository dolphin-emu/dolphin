// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main

import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag

/**
 * Abstraction for the screen that shows on application launch.
 * Implementations will differ primarily to target touch-screen
 * or non-touch screen devices.
 */
interface MainView {
    /**
     * Pass the view the native library's version string. Displaying
     * it is optional.
     *
     * @param version A string pulled from native code.
     */
    fun setVersionString(version: String)

    fun launchSettingsActivity(menuTag: MenuTag?)

    fun launchFileListActivity()

    fun launchOpenFileActivity(requestCode: Int)

    /**
     * Shows or hides the loading indicator.
     */
    fun setRefreshing(refreshing: Boolean)

    /**
     * To be called when the game file cache is updated.
     */
    fun showGames()

    fun reloadGrid()

    fun showGridOptions()
}
