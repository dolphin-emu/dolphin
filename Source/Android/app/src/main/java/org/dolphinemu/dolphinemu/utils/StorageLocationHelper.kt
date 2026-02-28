// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import androidx.preference.PreferenceManager
import java.io.File

object StorageLocationHelper {
    private const val KEY_STORAGE_CONFIGURED = "storage_configured"
    private const val KEY_CUSTOM_USER_DIRECTORY = "custom_user_directory"

    fun isStorageConfigured(context: Context): Boolean {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        if (prefs.getBoolean(KEY_STORAGE_CONFIGURED, false)) {
            return true
        }

        // Auto-migrate existing users: if Dolphin.ini already exists in the default path,
        // mark as configured so the onboarding dialog is not shown.
        val defaultPath = context.getExternalFilesDir(null)
        if (defaultPath != null) {
            val dolphinIni = File(defaultPath, "Config/Dolphin.ini")
            if (dolphinIni.exists()) {
                setStorageConfigured(context, null)
                return true
            }
        }

        return false
    }

    fun setStorageConfigured(context: Context, customPath: String?) {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        val editor = prefs.edit()
        editor.putBoolean(KEY_STORAGE_CONFIGURED, true)
        if (customPath != null) {
            editor.putString(KEY_CUSTOM_USER_DIRECTORY, customPath)
        } else {
            editor.remove(KEY_CUSTOM_USER_DIRECTORY)
        }
        editor.apply()
    }

    fun getCustomUserDirectoryPath(context: Context): String? {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        return prefs.getString(KEY_CUSTOM_USER_DIRECTORY, null)
    }

    fun getCurrentUserDirectoryDescription(context: Context): String {
        val customPath = getCustomUserDirectoryPath(context)
        if (customPath != null) {
            return customPath
        }
        val defaultPath = context.getExternalFilesDir(null)
        return defaultPath?.absolutePath ?: "Internal Storage"
    }
}
