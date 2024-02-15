// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentActivity
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.utils.GpuDriverInstallResult

/**
 * Abstraction for a screen showing a list of settings. Instances of
 * this type of view will each display a layer of the Setting hierarchy.
 */
interface SettingsFragmentView {
    /**
     * Called by the containing Activity to notify the Fragment that an
     * asynchronous load operation completed.
     *
     * @param settings The (possibly null) result of the ini load operation.
     */
    fun onSettingsFileLoaded(settings: Settings)

    /**
     * Pass an ArrayList of settings to the View so that it can be displayed on screen.
     *
     * @param settingsList The settings to display
     */
    fun showSettingsList(settingsList: ArrayList<SettingsItem>)

    /**
     * Called by the containing Activity when an asynchronous load operation fails.
     * Instructs the Fragment to load the settings screen with defaults selected.
     */
    fun loadDefaultSettings()

    /**
     * @return The Fragment's containing activity.
     */
    val fragmentActivity: FragmentActivity

    /**
     * @return The Fragment's SettingsAdapter.
     */
    val adapter: SettingsAdapter?

    /**
     * Tell the Fragment to tell the containing Activity to show a new
     * Fragment containing a submenu of settings.
     *
     * @param menuKey Identifier for the settings group that should be shown.
     */
    fun loadSubMenu(menuKey: MenuTag)
    fun showDialogFragment(fragment: DialogFragment)

    /**
     * Tell the Fragment to tell the containing activity to display a toast message.
     *
     * @param message Text to be shown in the Toast
     */
    fun showToastMessage(message: String)

    /**
     * @return The backing settings store.
     */
    val settings: Settings?

    /**
     * Have the fragment tell the containing Activity that a Setting was modified.
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
     * Have the fragment tell the containing Activity that the user wants to open the MenuTag
     * associated with a Setting.
     *
     * @param menuTag The MenuTag of the Setting.
     * @param value   The current value of the Setting.
     */
    fun onMenuTagAction(menuTag: MenuTag, value: Int)

    /**
     * Returns whether anything will happen when the user wants to open the MenuTag associated with a
     * stringSetting, given the current value of the Setting.
     *
     * @param menuTag The MenuTag of the Setting.
     * @param value   The current value of the Setting.
     */
    fun hasMenuTagActionForValue(menuTag: MenuTag, value: Int): Boolean
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
     */
    fun setOldControllerSettingsWarningVisibility(visible: Boolean)

    /**
     * Called when the driver installation is finished
     *
     * @param result The result of the driver installation
     */
    fun onDriverInstallDone(result: GpuDriverInstallResult)

    /**
     * Called when the driver uninstall process is finished
     */
    fun onDriverUninstallDone()

    /**
     * Shows a dialog asking the user to install or uninstall a GPU driver
     */
    fun showGpuDriverDialog()
}
