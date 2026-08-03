// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.asFlow
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.ConfigChangedCallback
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.model.GameFileCache
import org.dolphinemu.dolphinemu.ui.platform.Platform
import org.dolphinemu.dolphinemu.ui.platform.PlatformTab
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import java.util.Arrays

/**
 * Loads game list data in the background.
 */
object GameFileCacheManager {
    private var gameFileCache: GameFileCache? = null
    private val gameFiles = MutableLiveData(emptyArray<GameFile>())
    private var firstLoadDone = false
    private var recursiveScanEnabled = false

    private val loadInProgress = MutableLiveData(false)
    private val rescanInProgress = MutableLiveData(false)

    @JvmStatic
    fun getGameFiles(): LiveData<Array<GameFile>> {
        return gameFiles
    }

    @JvmStatic
    fun getGameFilesForPlatformTab(platformTab: PlatformTab): List<GameFile> {
        val allGames = gameFiles.value!!
        val platformTabGames = ArrayList<GameFile>()
        for (game in allGames) {
            if (Platform.fromInt(game.getPlatform()).toPlatformTab() == platformTab) {
                platformTabGames.add(game)
            }
        }
        return platformTabGames
    }

    @JvmStatic
    fun getGameFileByGameId(gameId: String): GameFile? {
        val allGames = gameFiles.value!!
        for (game in allGames) {
            if (game.getGameId() == gameId) {
                return game
            }
        }
        return null
    }

    @JvmStatic
    fun findSecondDisc(game: GameFile): GameFile? {
        var matchWithoutRevision: GameFile? = null

        val allGames = gameFiles.value!!
        for (otherGame in allGames) {
            if (game.getGameId() == otherGame.getGameId() && game.getDiscNumber() != otherGame.getDiscNumber()) {
                if (game.getRevision() == otherGame.getRevision()) {
                    return otherGame
                } else {
                    matchWithoutRevision = otherGame
                }
            }
        }

        return matchWithoutRevision
    }

    @JvmStatic
    fun findSecondDiscAndGetPaths(gameFile: GameFile?): Array<String> {
        val nonNullGameFile = gameFile!!
        val secondFile = findSecondDisc(nonNullGameFile)
        return if (secondFile == null) {
            arrayOf(nonNullGameFile.getPath())
        } else {
            arrayOf(nonNullGameFile.getPath(), secondFile.getPath())
        }
    }

    /**
     * Returns true if in the process of loading the cache for the first time.
     */
    @JvmStatic
    fun isLoading(): LiveData<Boolean> {
        return loadInProgress
    }

    /**
     * Returns true if in the process of rescanning.
     */
    @JvmStatic
    fun isRescanning(): LiveData<Boolean> {
        return rescanInProgress
    }

    @JvmStatic
    fun isLoadingOrRescanning(): Boolean {
        return loadInProgress.value!! || rescanInProgress.value!!
    }

    /**
     * Asynchronously loads the game file cache from disk, without checking
     * if the games are still present in the user's configured folders.
     * If this has already been called, calling it again has no effect.
     */
    @JvmStatic
    fun startLoad() {
        createGameFileCacheIfNeeded()

        if (!loadInProgress.value!!) {
            loadInProgress.value = true
            AfterDirectoryInitializationRunner().runWithoutLifecycle {
                MainScope().launch { load() }
            }
        }
    }

    /**
     * Asynchronously scans for games in the user's configured folders,
     * updating the game file cache with the results.
     * If loading the game file cache hasn't started or hasn't finished,
     * the execution of this will be postponed until it finishes.
     */
    @JvmStatic
    fun startRescan() {
        createGameFileCacheIfNeeded()

        if (!rescanInProgress.value!!) {
            rescanInProgress.value = true
            AfterDirectoryInitializationRunner().runWithoutLifecycle {
                MainScope().launch { rescan() }
            }
        }
    }

    @JvmStatic
    fun addOrGet(gamePath: String?): GameFile {
        val nonNullGamePath = gamePath!!

        // Common case: The game is in the cache, so we grab it from there. (GameFileCache.addOrGet
        // actually already checks for this case, but we want to avoid calling it if possible
        // because rescan() may hold a lock on gameFileCache for extended periods of time.)
        val allGames = gameFiles.value!!
        for (game in allGames) {
            if (game.getPath() == nonNullGamePath) {
                return game
            }
        }

        // Unusual case: The game wasn't found in the cache.
        // Scan the game and add it to the cache so that we can return it.
        createGameFileCacheIfNeeded()
        return gameFileCache!!.addOrGet(nonNullGamePath)!!
    }

    /**
     * Loads the game file cache from disk, without checking if the
     * games are still present in the user's configured folders.
     * If this has already been called, calling it again has no effect.
     */
    private suspend fun load() {
        if (!firstLoadDone) {
            withContext(Dispatchers.IO) {
                setUpAutomaticRescan()
                gameFileCache!!.load()
                if (gameFileCache!!.getSize() != 0) {
                    updateGameFileArray()
                }
            }
            firstLoadDone = true
        }

        loadInProgress.value = false
    }

    /**
     * Scans for games in the user's configured folders,
     * updating the game file cache with the results.
     * If load hasn't been called before this, the execution of this
     * will be postponed until after load runs.
     */
    private suspend fun rescan() {
        while (!firstLoadDone) {
            // Wait until loadInProgress becomes false
            loadInProgress.asFlow().first { !it }
        }

        withContext(Dispatchers.IO) {
            val gamePaths = GameFileCache.getAllGamePaths()

            val changed = gameFileCache!!.update(gamePaths)
            if (changed) {
                updateGameFileArray()
            }

            val additionalMetadataChanged = gameFileCache!!.updateAdditionalMetadata()
            if (additionalMetadataChanged) {
                updateGameFileArray()
            }

            if (changed || additionalMetadataChanged) {
                gameFileCache!!.save()
            }
        }

        rescanInProgress.value = false
    }

    private fun updateGameFileArray() {
        val gameFilesTemp = gameFileCache!!.getAllGames()
        Arrays.sort(gameFilesTemp) { lhs, rhs ->
            lhs.getTitle().compareTo(rhs.getTitle(), ignoreCase = true)
        }
        gameFiles.postValue(gameFilesTemp)
    }

    private fun createGameFileCacheIfNeeded() {
        // Creating the GameFileCache in the static initializer may be unsafe, because GameFileCache
        // relies on native code, and the native library isn't loaded right when the app starts.
        // We create it here instead.
        if (gameFileCache == null) {
            gameFileCache = GameFileCache()
        }
    }

    private fun setUpAutomaticRescan() {
        recursiveScanEnabled = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.boolean
        ConfigChangedCallback {
            MainScope().launch {
                val newRecursiveScanEnabled = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.boolean
                if (recursiveScanEnabled != newRecursiveScanEnabled) {
                    recursiveScanEnabled = newRecursiveScanEnabled
                    startRescan()
                }
            }
        }
    }
}
