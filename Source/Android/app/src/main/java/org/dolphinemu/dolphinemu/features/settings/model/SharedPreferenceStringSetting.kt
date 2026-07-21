// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import android.content.Context
import androidx.core.content.edit
import androidx.preference.PreferenceManager
import org.dolphinemu.dolphinemu.DolphinApplication

class SharedPreferenceStringSetting(
    private val key: String,
    private val defaultValue: String = ""
) : AbstractStringSetting {
    private val context: Context
        get() = DolphinApplication.getAppContext()

    override val isOverridden: Boolean
        get() = false

    override val isRuntimeEditable: Boolean
        get() = true

    override fun delete(settings: Settings): Boolean {
        PreferenceManager.getDefaultSharedPreferences(context).edit { remove(key) }
        return true
    }

    override val string: String
        get() = PreferenceManager.getDefaultSharedPreferences(context)
            .getString(key, defaultValue)
            .orEmpty()

    override fun setString(settings: Settings, newValue: String) {
        PreferenceManager.getDefaultSharedPreferences(context).edit { putString(key, newValue) }
    }

    companion object {
        private const val GB_PLAYER_BOOT_PATH_KEY = "DualScreenGBPlayerBootPath"
        val MAIN_ANDROID_GB_PLAYER_BOOT_PATH =
            SharedPreferenceStringSetting(GB_PLAYER_BOOT_PATH_KEY)
    }
}
