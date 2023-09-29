// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import androidx.annotation.Keep
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.utils.ContentHandler
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
            val paths = getIsoPaths()

            if (!paths.contains(path)) {
                setIsoPaths(paths + path)
                NativeConfig.save(NativeConfig.LAYER_BASE)
            }
        }

        private fun getFolderPaths(removeNonExistentFolders: Boolean): Array<String> {
            val paths = getIsoPaths()

            val filteredPaths = paths.filter {path ->
                if (ContentHandler.isContentUri(path))
                    ContentHandler.exists(path)
                else
                    File(path).exists()
            }.toTypedArray()

            if (removeNonExistentFolders && paths.size > filteredPaths.size) {
                setIsoPaths(filteredPaths)
                NativeConfig.save(NativeConfig.LAYER_BASE)
            }

            return filteredPaths
        }

        @JvmStatic
        fun getAllGamePaths(): Array<String> {
            val recursiveScan = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.boolean
            val folderPaths = getFolderPaths(true)
            return getAllGamePaths(folderPaths, recursiveScan)
        }

        @JvmStatic
        external fun getAllGamePaths(
            folderPaths: Array<String>,
            recursiveScan: Boolean
        ): Array<String>

        @JvmStatic
        external fun getIsoPaths(): Array<String>

        @JvmStatic
        external fun setIsoPaths(paths: Array<String>)
    }
}
