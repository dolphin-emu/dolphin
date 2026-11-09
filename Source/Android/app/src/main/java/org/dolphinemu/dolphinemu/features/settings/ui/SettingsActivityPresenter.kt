// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.os.Bundle
import android.text.TextUtils
import androidx.appcompat.app.AppCompatActivity
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import org.dolphinemu.dolphinemu.utils.Log

class SettingsActivityPresenter(
    private val activityView: SettingsActivityView, var settings: Settings?
) {
    private var menuTag: MenuTag? = null
    private var gameId: String? = null
    private var revision = 0
    private var isWii = false
    private lateinit var activity: AppCompatActivity
    var settingsSearchQuery = ""
        private set
    var isSettingsSearchActive = false
        private set

    fun onCreate(
        savedInstanceState: Bundle?,
        menuTag: MenuTag?,
        gameId: String?,
        revision: Int,
        isWii: Boolean,
        activity: AppCompatActivity
    ) {
        this.menuTag = menuTag
        this.gameId = gameId
        this.revision = revision
        this.isWii = isWii
        this.activity = activity
        if (savedInstanceState != null) {
            isSettingsSearchActive =
                savedInstanceState.getBoolean(KEY_SETTINGS_SEARCH_ACTIVE)
            settingsSearchQuery =
                savedInstanceState.getString(KEY_SETTINGS_SEARCH_QUERY).orEmpty()
        }
    }

    fun onSaveInstanceState(outState: Bundle) {
        outState.putBoolean(KEY_SETTINGS_SEARCH_ACTIVE, isSettingsSearchActive)
        outState.putString(KEY_SETTINGS_SEARCH_QUERY, settingsSearchQuery)
    }

    fun onSettingsSearchQueryChanged(query: String) {
        settingsSearchQuery = query
        activityView.filterSettings(query)
    }

    fun enterSettingsSearch(): Boolean {
        if (isSettingsSearchActive) {
            return false
        }

        isSettingsSearchActive = true
        activityView.filterSettings(settingsSearchQuery)
        return true
    }

    fun exitSettingsSearch(): Boolean {
        if (!isSettingsSearchActive) {
            return false
        }

        isSettingsSearchActive = false
        settingsSearchQuery = ""
        activityView.filterSettings("")
        return true
    }

    fun onDestroy() {
        if (settings != null) {
            settings!!.close()
            settings = null
        }
    }

    fun onStart() {
        prepareDolphinDirectoriesIfNeeded()
    }

    private fun loadSettingsUI() {
        activityView.hideLoading()
        if (!settings!!.areSettingsLoaded()) {
            if (!TextUtils.isEmpty(gameId)) {
                settings!!.loadSettings(gameId!!, revision, isWii)
                if (settings!!.gameIniContainsJunk()) {
                    activityView.showGameIniJunkDeletionQuestion()
                }
            } else {
                settings!!.loadSettings(isWii)
            }
        }
        activityView.showSettingsFragment(menuTag!!, null, false, gameId!!)
        activityView.onSettingsFileLoaded(settings!!)
    }

    private fun prepareDolphinDirectoriesIfNeeded() {
        activityView.showLoading()
        AfterDirectoryInitializationRunner().runWithLifecycle(activity) { loadSettingsUI() }
    }

    fun clearGameSettings() {
        settings!!.clearGameSettings()
        onSettingChanged()
    }

    fun onStop(finishing: Boolean) {
        if (settings != null && finishing && settings!!.areSettingsLoaded()) {
            Log.debug("[SettingsActivity] Settings activity stopping. Ensuring settings are saved.")
            settings!!.saveSettings()
        }
    }

    fun onSettingChanged() {
        if (settings != null && settings!!.areSettingsLoaded()) {
            settings!!.saveSettings()
        }
    }

    fun onMenuTagAction(menuTag: MenuTag, value: Int) {
        val action = getMenuTagAction(menuTag, value) ?: return
        activityView.showSettingsFragment(action.menuTag, action.extras, true, gameId!!)
    }

    fun hasMenuTagActionForValue(menuTag: MenuTag, value: Int): Boolean {
        return getMenuTagAction(menuTag, value) != null
    }

    fun getMenuTagActionExtras(menuTag: MenuTag, value: Int): Bundle? {
        return getMenuTagAction(menuTag, value)?.extras
    }

    private fun getMenuTagAction(menuTag: MenuTag, value: Int): MenuTagAction? {
        return when {
            // Not disabled or dummy
            menuTag.isSerialPort1Menu && value != 0 && value != 255 -> MenuTagAction(
                menuTag, Bundle().apply {
                    putInt(SettingsFragmentPresenter.ARG_SERIALPORT1_TYPE, value)
                })

            // Not disabled
            menuTag.isGCPadMenu && value != 0 -> MenuTagAction(
                menuTag, Bundle().apply {
                    putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value)
                })

            // Emulated Wii Remote
            menuTag.isWiimoteMenu && value == 1 -> MenuTagAction(menuTag, null)

            // Not disabled
            menuTag.isWiimoteExtensionMenu && value != 0 -> MenuTagAction(
                menuTag, Bundle().apply {
                    putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value)
                })

            else -> null
        }
    }

    private data class MenuTagAction(val menuTag: MenuTag, val extras: Bundle?)

    companion object {
        private const val KEY_SETTINGS_SEARCH_ACTIVE = "settings_search_active"
        private const val KEY_SETTINGS_SEARCH_QUERY = "settings_search_query"
    }
}
