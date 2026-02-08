// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.os.Bundle
import android.text.TextUtils
import androidx.appcompat.app.AppCompatActivity
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import org.dolphinemu.dolphinemu.utils.Log

class SettingsActivityPresenter(
    private val activityView: SettingsActivityView,
    var settings: Settings?
) {
    private var shouldSave = false
    private var menuTag: MenuTag? = null
    private var gameId: String? = null
    private var revision = 0
    private var isWii = false
    private lateinit var activity: AppCompatActivity

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
        shouldSave = savedInstanceState != null && savedInstanceState.getBoolean(KEY_SHOULD_SAVE)
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
        if (settings != null && finishing && shouldSave) {
            Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...")
            settings!!.saveSettings(activity)
        }
    }

    fun onSettingChanged() {
        shouldSave = true
    }

    fun saveState(outState: Bundle) {
        outState.putBoolean(KEY_SHOULD_SAVE, shouldSave)
    }

    fun onMenuTagAction(menuTag: MenuTag, value: Int) {
        if (menuTag.isSerialPort1Menu) {
            // Not disabled or dummy
            if (value != 0 && value != 255) {
                val bundle = Bundle()
                bundle.putInt(SettingsFragmentPresenter.ARG_SERIALPORT1_TYPE, value)
                activityView.showSettingsFragment(menuTag, bundle, true, gameId!!)
            }
        }
        if (menuTag.isGCPadMenu) {
            // Not disabled
            if (value != 0)
            {
                val bundle = Bundle()
                bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value)
                activityView.showSettingsFragment(menuTag, bundle, true, gameId!!)
            }
        }
        if (menuTag.isWiimoteMenu) {
            // Emulated Wii Remote
            if (value == 1) {
                activityView.showSettingsFragment(menuTag, null, true, gameId!!)
            }
        }
        if (menuTag.isWiimoteExtensionMenu) {
            // Not disabled
            if (value != 0) {
                val bundle = Bundle()
                bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value)
                activityView.showSettingsFragment(menuTag, bundle, true, gameId!!)
            }
        }
    }

    fun hasMenuTagActionForValue(menuTag: MenuTag, value: Int): Boolean {
        if (menuTag.isSerialPort1Menu) {
            // Not disabled or dummy
            return value != 0 && value != 255
        }
        if (menuTag.isGCPadMenu) {
            // Not disabled
            return value != 0
        }
        if (menuTag.isWiimoteMenu) {
            // Emulated Wii Remote
            return value == 1
        }
        return if (menuTag.isWiimoteExtensionMenu) {
            // Not disabled
            value != 0
        } else false
    }

    companion object {
        private const val KEY_SHOULD_SAVE = "should_save"
    }
}
