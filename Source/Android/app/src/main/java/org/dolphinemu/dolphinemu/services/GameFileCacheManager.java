// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services;

import android.os.Handler;
import android.os.Looper;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.ConfigChangedCallback;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Loads game list data on a separate thread.
 */
public final class GameFileCacheManager
{
  private static GameFileCache sGameFileCache = null;
  private static final MutableLiveData<GameFile[]> sGameFiles =
          new MutableLiveData<>(new GameFile[]{});
  private static boolean sFirstLoadDone = false;
  private static boolean sRunRescanAfterLoad = false;
  private static boolean sRecursiveScanEnabled;

  private static final ExecutorService sExecutor = Executors.newFixedThreadPool(1);
  private static final MutableLiveData<Boolean> sLoadInProgress = new MutableLiveData<>(false);
  private static final MutableLiveData<Boolean> sRescanInProgress = new MutableLiveData<>(false);

  private GameFileCacheManager()
  {
  }

  public static LiveData<GameFile[]> getGameFiles()
  {
    return sGameFiles;
  }

  public static List<GameFile> getGameFilesForPlatform(Platform platform)
  {
    GameFile[] allGames = sGameFiles.getValue();
    ArrayList<GameFile> platformGames = new ArrayList<>();
    for (GameFile game : allGames)
    {
      if (Platform.fromNativeInt(game.getPlatform()) == platform)
      {
        platformGames.add(game);
      }
    }
    return platformGames;
  }

  public static GameFile getGameFileByGameId(String gameId)
  {
    GameFile[] allGames = sGameFiles.getValue();
    for (GameFile game : allGames)
    {
      if (game.getGameId().equals(gameId))
      {
        return game;
      }
    }
    return null;
  }

  public static GameFile findSecondDisc(GameFile game)
  {
    GameFile matchWithoutRevision = null;

    GameFile[] allGames = sGameFiles.getValue();
    for (GameFile otherGame : allGames)
    {
      if (game.getGameId().equals(otherGame.getGameId()) &&
              game.getDiscNumber() != otherGame.getDiscNumber())
      {
        if (game.getRevision() == otherGame.getRevision())
          return otherGame;
        else
          matchWithoutRevision = otherGame;
      }
    }

    return matchWithoutRevision;
  }

  public static String[] findSecondDiscAndGetPaths(GameFile gameFile)
  {
    GameFile secondFile = findSecondDisc(gameFile);
    if (secondFile == null)
      return new String[]{gameFile.getPath()};
    else
      return new String[]{gameFile.getPath(), secondFile.getPath()};
  }

  /**
   * Returns true if in the process of loading the cache for the first time.
   */
  public static LiveData<Boolean> isLoading()
  {
    return sLoadInProgress;
  }

  /**
   * Returns true if in the process of rescanning.
   */
  public static LiveData<Boolean> isRescanning()
  {
    return sRescanInProgress;
  }

  public static boolean isLoadingOrRescanning()
  {
    return sLoadInProgress.getValue() || sRescanInProgress.getValue();
  }

  /**
   * Asynchronously loads the game file cache from disk, without checking
   * if the games are still present in the user's configured folders.
   * If this has already been called, calling it again has no effect.
   */
  public static void startLoad()
  {
    createGameFileCacheIfNeeded();

    if (!sLoadInProgress.getValue())
    {
      sLoadInProgress.setValue(true);
      new AfterDirectoryInitializationRunner().runWithoutLifecycle(
              () -> sExecutor.execute(GameFileCacheManager::load));
    }
  }

  /**
   * Asynchronously scans for games in the user's configured folders,
   * updating the game file cache with the results.
   * If loading the game file cache hasn't started or hasn't finished,
   * the execution of this will be postponed until it finishes.
   */
  public static void startRescan()
  {
    createGameFileCacheIfNeeded();

    if (!sRescanInProgress.getValue())
    {
      sRescanInProgress.setValue(true);
      new AfterDirectoryInitializationRunner().runWithoutLifecycle(
              () -> sExecutor.execute(GameFileCacheManager::rescan));
    }
  }

  public static GameFile addOrGet(String gamePath)
  {
    // Common case: The game is in the cache, so just grab it from there. (GameFileCache.addOrGet
    // actually already checks for this case, but we want to avoid calling it if possible
    // because the executor thread may hold a lock on sGameFileCache for extended periods of time.)
    GameFile[] allGames = sGameFiles.getValue();
    for (GameFile game : allGames)
    {
      if (game.getPath().equals(gamePath))
      {
        return game;
      }
    }

    // Unusual case: The game wasn't found in the cache.
    // Scan the game and add it to the cache so that we can return it.
    createGameFileCacheIfNeeded();
    return sGameFileCache.addOrGet(gamePath);
  }

  /**
   * Loads the game file cache from disk, without checking if the
   * games are still present in the user's configured folders.
   * If this has already been called, calling it again has no effect.
   */
  private static void load()
  {
    if (!sFirstLoadDone)
    {
      sFirstLoadDone = true;
      setUpAutomaticRescan();
      sGameFileCache.load();
      if (sGameFileCache.getSize() != 0)
      {
        updateGameFileArray();
      }
    }

    if (sRunRescanAfterLoad)
    {
      // Without this, there will be a short blip where the loading indicator in the GUI disappears
      // because neither sLoadInProgress nor sRescanInProgress is true
      sRescanInProgress.postValue(true);
    }

    sLoadInProgress.postValue(false);

    if (sRunRescanAfterLoad)
    {
      sRunRescanAfterLoad = false;
      rescan();
    }
  }

  /**
   * Scans for games in the user's configured folders,
   * updating the game file cache with the results.
   * If load hasn't been called before this, the execution of this
   * will be postponed until after load runs.
   */
  private static void rescan()
  {
    if (!sFirstLoadDone)
    {
      sRunRescanAfterLoad = true;
    }
    else
    {
      String[] gamePaths = GameFileCache.getAllGamePaths();

      boolean changed = sGameFileCache.update(gamePaths);
      if (changed)
      {
        updateGameFileArray();
      }

      boolean additionalMetadataChanged = sGameFileCache.updateAdditionalMetadata();
      if (additionalMetadataChanged)
      {
        updateGameFileArray();
      }

      if (changed || additionalMetadataChanged)
      {
        sGameFileCache.save();
      }
    }

    sRescanInProgress.postValue(false);
  }

  private static void updateGameFileArray()
  {
    GameFile[] gameFilesTemp = sGameFileCache.getAllGames();
    Arrays.sort(gameFilesTemp, (lhs, rhs) -> lhs.getTitle().compareToIgnoreCase(rhs.getTitle()));
    sGameFiles.postValue(gameFilesTemp);
  }

  private static void createGameFileCacheIfNeeded()
  {
    // Creating the GameFileCache in the static initializer may be unsafe, because GameFileCache
    // relies on native code, and the native library isn't loaded right when the app starts.
    // We create it here instead.

    if (sGameFileCache == null)
    {
      sGameFileCache = new GameFileCache();
    }
  }

  private static void setUpAutomaticRescan()
  {
    sRecursiveScanEnabled = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBoolean();
    new ConfigChangedCallback(() ->
            new Handler(Looper.getMainLooper()).post(() ->
            {
              boolean recursiveScanEnabled = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBoolean();
              if (sRecursiveScanEnabled != recursiveScanEnabled)
              {
                sRecursiveScanEnabled = recursiveScanEnabled;
                startRescan();
              }
            }));
  }
}
