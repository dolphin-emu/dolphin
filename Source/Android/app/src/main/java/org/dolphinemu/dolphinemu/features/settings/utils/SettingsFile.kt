// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.utils

import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivityView
import org.dolphinemu.dolphinemu.utils.BiMap
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.IniFile
import org.dolphinemu.dolphinemu.utils.Log
import java.io.File

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val KEY_ISO_PATH_BASE = "ISOPath"
    const val KEY_ISO_PATHS = "ISOPaths"
    private val sectionsMap = BiMap<String?, String?>()

    init {
        sectionsMap.apply {
            add("Hardware", "Video_Hardware")
            add("Settings", "Video_Settings")
            add("Enhancements", "Video_Enhancements")
            add("Stereoscopy", "Video_Stereoscopy")
            add("Hacks", "Video_Hacks")
            add("GameSpecific", "Video")
        }
    }

    /**
     * Reads a given .ini file from disk and returns it.
     * If unsuccessful, outputs an error telling why it failed.
     *
     * @param file The ini file to load the settings from
     * @param ini  The object to load into
     * @param view The current view.
     */
    private fun readFile(file: File, ini: IniFile, view: SettingsActivityView) {
        if (!ini.load(file, true)) {
            Log.error("[SettingsFile] Error reading from: " + file.absolutePath)
            view.onSettingsFileNotFound()
        }
    }

    fun readFile(fileName: String, ini: IniFile, view: SettingsActivityView) {
        readFile(getSettingsFile(fileName), ini, view)
    }

    /**
     * Reads a given .ini file from disk and returns it.
     * If unsuccessful, outputs an error telling why it failed.
     *
     * @param gameId the id of the game to load settings for.
     * @param ini    The object to load into
     * @param view   The current view.
     */
    fun readCustomGameSettings(
        gameId: String,
        ini: IniFile,
        view: SettingsActivityView
    ) {
        readFile(getCustomGameSettingsFile(gameId), ini, view)
    }

    /**
     * Saves a given .ini file on disk.
     * If unsuccessful, outputs an error telling why it failed.
     *
     * @param fileName The target filename without a path or extension.
     * @param ini      The IniFile we want to serialize.
     * @param view     The current view.
     */
    fun saveFile(fileName: String, ini: IniFile, view: SettingsActivityView) {
        if (!ini.save(getSettingsFile(fileName))) {
            Log.error("[SettingsFile] Error saving to: $fileName.ini")
            view.showToastMessage("Error saving $fileName.ini")
        }
    }

    fun saveCustomGameSettings(gameId: String, ini: IniFile) {
        ini.save(getCustomGameSettingsFile(gameId))
    }

    fun mapSectionNameFromIni(generalSectionName: String): String? {
        return if (sectionsMap.getForward(generalSectionName) != null) {
            sectionsMap.getForward(generalSectionName)
        } else generalSectionName
    }

    fun mapSectionNameToIni(generalSectionName: String): String? {
        return if (sectionsMap.getBackward(generalSectionName) != null) {
            sectionsMap.getBackward(generalSectionName)
        } else generalSectionName
    }

    @JvmStatic
    fun getSettingsFile(fileName: String): File {
        return File(DirectoryInitialization.getUserDirectory() + "/Config/" + fileName + ".ini")
    }

    private fun getCustomGameSettingsFile(gameId: String): File {
        return File(
            DirectoryInitialization.getUserDirectory() + "/GameSettings/" + gameId + ".ini"
        )
    }
}
