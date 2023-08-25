// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import androidx.annotation.Keep
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile
import org.dolphinemu.dolphinemu.utils.ContentHandler
import org.dolphinemu.dolphinemu.utils.IniFile
import java.io.File

class GameFileCache {
    @Keep
    private val pointer: Long = newGameFileCache()

    external fun finalize()

    @Synchronized
    external fun getSize(): Int

    @Synchronized
    external fun getAllGames(): Array<GameFile>

    @Synchronized
    external fun addOrGet(gamePath: String): GameFile?

    /**
     * Sets the list of games to cache.
     *
     * Games which are in the passed-in list but not in the cache are scanned and added to the cache,
     * and games which are in the cache but not in the passed-in list are removed from the cache.
     *
     * @return true if the cache was modified
     */
    @Synchronized
    external fun update(gamePaths: Array<String>): Boolean

    /**
     * For each game that already is in the cache, scans the folder that contains the game
     * for additional metadata files (PNG/XML).
     *
     * @return true if the cache was modified
     */
    @Synchronized
    external fun updateAdditionalMetadata(): Boolean

    @Synchronized
    external fun load(): Boolean

    @Synchronized
    external fun save(): Boolean

    companion object {
        @JvmStatic
        private external fun newGameFileCache(): Long

        fun addGameFolder(path: String) {
            val dolphinFile = SettingsFile.getSettingsFile(Settings.FILE_DOLPHIN)
            val dolphinIni = IniFile(dolphinFile)
            val pathSet = getPathSet(false)
            val totalISOPaths =
                dolphinIni.getInt(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATHS, 0)

            if (!pathSet.contains(path)) {
                dolphinIni.setInt(
                    Settings.SECTION_INI_GENERAL,
                    SettingsFile.KEY_ISO_PATHS,
                    totalISOPaths + 1
                )
                dolphinIni.setString(
                    Settings.SECTION_INI_GENERAL,
                    SettingsFile.KEY_ISO_PATH_BASE + totalISOPaths,
                    path
                )
                dolphinIni.save(dolphinFile)
                NativeLibrary.ReloadConfig()
            }
        }

        private fun getPathSet(removeNonExistentFolders: Boolean): LinkedHashSet<String> {
            val dolphinFile = SettingsFile.getSettingsFile(Settings.FILE_DOLPHIN)
            val dolphinIni = IniFile(dolphinFile)
            val pathSet = LinkedHashSet<String>()
            val totalISOPaths =
                dolphinIni.getInt(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATHS, 0)

            for (i in 0 until totalISOPaths) {
                val path = dolphinIni.getString(
                    Settings.SECTION_INI_GENERAL,
                    SettingsFile.KEY_ISO_PATH_BASE + i,
                    ""
                )

                val pathExists = if (ContentHandler.isContentUri(path))
                    ContentHandler.exists(path)
                else
                    File(path).exists()
                if (pathExists) {
                    pathSet.add(path)
                }
            }

            if (removeNonExistentFolders && totalISOPaths > pathSet.size) {
                var setIndex = 0

                dolphinIni.setInt(
                    Settings.SECTION_INI_GENERAL,
                    SettingsFile.KEY_ISO_PATHS,
                    pathSet.size
                )

                // One or more folders have been removed.
                for (entry in pathSet) {
                    dolphinIni.setString(
                        Settings.SECTION_INI_GENERAL,
                        SettingsFile.KEY_ISO_PATH_BASE + setIndex,
                        entry
                    )
                    setIndex++
                }

                // Delete known unnecessary keys. Ignore i values beyond totalISOPaths.
                for (i in setIndex until totalISOPaths) {
                    dolphinIni.deleteKey(
                        Settings.SECTION_INI_GENERAL,
                        SettingsFile.KEY_ISO_PATH_BASE + i
                    )
                }

                dolphinIni.save(dolphinFile)
                NativeLibrary.ReloadConfig()
            }
            return pathSet
        }

        @JvmStatic
        fun getAllGamePaths(): Array<String> {
            val recursiveScan = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.boolean
            val folderPathsSet = getPathSet(true)
            val folderPaths = folderPathsSet.toTypedArray()
            return getAllGamePaths(folderPaths, recursiveScan)
        }

        @JvmStatic
        external fun getAllGamePaths(
            folderPaths: Array<String>,
            recursiveScan: Boolean
        ): Array<String>
    }
}
