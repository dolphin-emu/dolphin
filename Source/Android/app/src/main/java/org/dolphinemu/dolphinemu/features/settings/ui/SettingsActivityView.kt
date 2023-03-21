// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.os.Bundle
import androidx.fragment.app.DialogFragment
import org.dolphinemu.dolphinemu.features.settings.model.Settings

/**
 * Abstraction for the Activity that manages SettingsFragments.
 */
interface SettingsActivityView {
    /**
     * Show a new SettingsFragment.
     *
     * @param menuTag    Identifier for the settings group that should be displayed.
     * @param addToStack Whether or not this fragment should replace a previous one.
     */
    fun showSettingsFragment(
        menuTag: MenuTag,
        extras: Bundle?,
        addToStack: Boolean,
        gameId: String
    )

    /**
     * Shows a DialogFragment.
     *
     * Only one can be shown at a time.
     */
    fun showDialogFragment(fragment: DialogFragment)

    /**
     * Called by a contained Fragment to get access to the Setting HashMap
     * loaded from disk, so that each Fragment doesn't need to perform its own
     * read operation.
     *
     * @return A possibly null HashMap of Settings.
     */
    val settings: Settings

    /**
     * Called when an asynchronous load operation completes.
     *
     * @param settings The (possibly null) result of the ini load operation.
     */
    fun onSettingsFileLoaded(settings: Settings)

    /**
     * Called when an asynchronous load operation fails.
     */
    fun onSettingsFileNotFound()

    /**
     * Display a popup text message on screen.
     *
     * @param message The contents of the onscreen message.
     */
    fun showToastMessage(message: String)

    /**
     * End the activity.
     */
    fun finish()

    /**
     * Called by a containing Fragment to tell the Activity that a Setting was changed;
     * unless this has been called, the Activity will not save to disk.
     */
    fun onSettingChanged()

    /**
     * Refetches the values of all controller settings.
     *
     * To be used when loading an input profile or performing some other action that changes all
     * controller settings at once.
     */
    fun onControllerSettingsChanged()

    /**
     * Called by a containing Fragment to tell the containing Activity that the user wants to open the
     * MenuTag associated with a Setting.
     *
     * @param menuTag The MenuTag of the Setting.
     * @param value   The current value of the Setting.
     */
    fun onMenuTagAction(menuTag: MenuTag, value: Int)

    /**
     * Returns whether anything will happen when the user wants to open the MenuTag associated with a
     * Setting, given the current value of the Setting.
     *
     * @param menuTag The MenuTag of the Setting.
     * @param value   The current value of the Setting.
     */
    fun hasMenuTagActionForValue(menuTag: MenuTag, value: Int): Boolean

    /**
     * Show loading dialog while loading the settings
     */
    fun showLoading()

    /**
     * Hide the loading dialog
     */
    fun hideLoading()

    /**
     * Tell the user that there is junk in the game INI and ask if they want to delete the whole file.
     */
    fun showGameIniJunkDeletionQuestion()

    /**
     * Accesses the material toolbar layout and changes the title
     */
    fun setToolbarTitle(title: String)
    /**
     * Returns whether the input mapping dialog should detect inputs from all devices,
     * not just the device configured for the controller.
     */
    /**
     * Sets whether the input mapping dialog should detect inputs from all devices,
     * not just the device configured for the controller.
     */
    var isMappingAllDevices: Boolean

    /**
     * Shows or hides a warning telling the user that they're using incompatible controller settings.
     * The warning is hidden by default.
     *
     * @param visible Whether the warning should be visible.
     * @return The height of the warning view, or 0 if the view is now invisible.
     */
    fun setOldControllerSettingsWarningVisibility(visible: Boolean): Int
}
