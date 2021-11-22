// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services;

import android.content.Context;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

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
  private static GameFileCache gameFileCache = null;
  private static final MutableLiveData<GameFile[]> gameFiles =
          new MutableLiveData<>(new GameFile[]{});
  private static boolean runRescanAfterLoad = false;

  private static final ExecutorService executor = Executors.newFixedThreadPool(1);
  private static final MutableLiveData<Boolean> loadInProgress = new MutableLiveData<>(false);
  private static final MutableLiveData<Boolean> rescanInProgress = new MutableLiveData<>(false);

  private GameFileCacheManager()
  {
  }

  public static LiveData<GameFile[]> getGameFiles()
  {
    return gameFiles;
  }

  public static List<GameFile> getGameFilesForPlatform(Platform platform)
  {
    GameFile[] allGames = gameFiles.getValue();
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
    GameFile[] allGames = gameFiles.getValue();
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

    GameFile[] allGames = gameFiles.getValue();
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
    return loadInProgress;
  }

  /**
   * Returns true if in the process of rescanning.
   */
  public static LiveData<Boolean> isRescanning()
  {
    return rescanInProgress;
  }

  public static boolean isLoadingOrRescanning()
  {
    return loadInProgress.getValue() || rescanInProgress.getValue();
  }

  /**
   * Asynchronously loads the game file cache from disk, without checking
   * if the games are still present in the user's configured folders.
   * If this has already been called, calling it again has no effect.
   */
  public static void startLoad(Context context)
  {
    if (!loadInProgress.getValue())
    {
      loadInProgress.setValue(true);
      new AfterDirectoryInitializationRunner().runWithoutLifecycle(context, false,
              () -> executor.execute(GameFileCacheManager::load));
    }
  }

  /**
   * Asynchronously scans for games in the user's configured folders,
   * updating the game file cache with the results.
   * If loading the game file cache hasn't started or hasn't finished,
   * the execution of this will be postponed until it finishes.
   */
  public static void startRescan(Context context)
  {
    if (!rescanInProgress.getValue())
    {
      rescanInProgress.setValue(true);
      new AfterDirectoryInitializationRunner().runWithoutLifecycle(context, false,
              () -> executor.execute(GameFileCacheManager::rescan));
    }
  }

  public static GameFile addOrGet(String gamePath)
  {
    // Common case: The game is in the cache, so just grab it from there.
    // (Actually, addOrGet already checks for this case, but we want to avoid calling it if possible
    // because onHandleIntent may hold a lock on gameFileCache for extended periods of time.)
    GameFile[] allGames = gameFiles.getValue();
    for (GameFile game : allGames)
    {
      if (game.getPath().equals(gamePath))
      {
        return game;
      }
    }

    // Unusual case: The game wasn't found in the cache.
    // Scan the game and add it to the cache so that we can return it.
    synchronized (gameFileCache)
    {
      return gameFileCache.addOrGet(gamePath);
    }
  }

  /**
   * Loads the game file cache from disk, without checking if the
   * games are still present in the user's configured folders.
   * If this has already been called, calling it again has no effect.
   */
  private static void load()
  {
    if (gameFileCache == null)
    {
      GameFileCache temp = new GameFileCache();
      synchronized (temp)
      {
        gameFileCache = temp;
        gameFileCache.load();
        if (gameFileCache.getSize() != 0)
        {
          updateGameFileArray();
        }
      }
    }

    if (runRescanAfterLoad)
    {
      rescanInProgress.postValue(true);
    }

    loadInProgress.postValue(false);

    if (runRescanAfterLoad)
    {
      runRescanAfterLoad = false;
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
    if (gameFileCache == null)
    {
      runRescanAfterLoad = true;
    }
    else
    {
      String[] gamePaths = GameFileCache.getAllGamePaths();

      boolean changed;
      synchronized (gameFileCache)
      {
        changed = gameFileCache.update(gamePaths);
      }
      if (changed)
      {
        updateGameFileArray();
      }

      boolean additionalMetadataChanged = gameFileCache.updateAdditionalMetadata();
      if (additionalMetadataChanged)
      {
        updateGameFileArray();
      }

      if (changed || additionalMetadataChanged)
      {
        gameFileCache.save();
      }
    }

    rescanInProgress.postValue(false);
  }

  private static void updateGameFileArray()
  {
    GameFile[] gameFilesTemp = gameFileCache.getAllGames();
    Arrays.sort(gameFilesTemp, (lhs, rhs) -> lhs.getTitle().compareToIgnoreCase(rhs.getTitle()));
    gameFiles.postValue(gameFilesTemp);
  }
}
