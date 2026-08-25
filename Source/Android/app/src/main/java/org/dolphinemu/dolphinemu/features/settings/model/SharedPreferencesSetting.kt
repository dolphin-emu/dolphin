// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import android.content.Context
import androidx.preference.PreferenceManager

class SharedPreferencesBooleanSetting(
    private val context: Context,
    private val key: String,
    private val defaultValue: Boolean
) : AbstractBooleanSetting {
    override val isOverridden: Boolean = false
    override val isRuntimeEditable: Boolean = true

    override fun delete(settings: Settings): Boolean {
        PreferenceManager.getDefaultSharedPreferences(context).edit().remove(key).apply()
        return true
    }

    override val boolean: Boolean
        get() = PreferenceManager.getDefaultSharedPreferences(context).getBoolean(key, defaultValue)

    override fun setBoolean(settings: Settings, newValue: Boolean) {
        PreferenceManager.getDefaultSharedPreferences(context).edit().putBoolean(key, newValue).apply()
    }
}

class SharedPreferencesStringSetting(
    private val context: Context,
    private val key: String,
    private val defaultValue: String
) : AbstractStringSetting {
    override val isOverridden: Boolean = false
    override val isRuntimeEditable: Boolean = true

    override fun delete(settings: Settings): Boolean {
        PreferenceManager.getDefaultSharedPreferences(context).edit().remove(key).apply()
        return true
    }

    override val string: String
        get() = PreferenceManager.getDefaultSharedPreferences(context).getString(key, defaultValue) ?: defaultValue

    override fun setString(settings: Settings, newValue: String) {
        PreferenceManager.getDefaultSharedPreferences(context).edit().putString(key, newValue).apply()
    }
}
