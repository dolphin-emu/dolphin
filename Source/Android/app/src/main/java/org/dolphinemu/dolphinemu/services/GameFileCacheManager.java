// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services;

import android.content.Context;
import android.content.Intent;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Loads game list data on a separate thread.
 */
public final class GameFileCacheManager
{
  /**
   * This is broadcast when the contents of the cache change.
   */
  public static final String CACHE_UPDATED = "org.dolphinemu.dolphinemu.GAME_FILE_CACHE_UPDATED";

  /**
   * This is broadcast when the service is done with all requested work, regardless of whether
   * the contents of the cache actually changed. (Maybe the cache was already up to date.)
   */
  public static final String DONE_LOADING =
          "org.dolphinemu.dolphinemu.GAME_FILE_CACHE_DONE_LOADING";

  private static GameFileCache gameFileCache = null;
  private static final AtomicReference<GameFile[]> gameFiles =
          new AtomicReference<>(new GameFile[]{});

  private static final ExecutorService executor = Executors.newFixedThreadPool(1);
  private static final AtomicBoolean loadInProgress = new AtomicBoolean(false);
  private static final AtomicBoolean rescanInProgress = new AtomicBoolean(false);

  private GameFileCacheManager()
  {
  }

  public static List<GameFile> getGameFilesForPlatform(Platform platform)
  {
    GameFile[] allGames = gameFiles.get();
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
    GameFile[] allGames = gameFiles.get();
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

    GameFile[] allGames = gameFiles.get();
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
   * Returns true if in the process of either loading the cache or rescanning.
   */
  public static boolean isLoading()
  {
    return loadInProgress.get();
  }

  /**
   * Returns true if in the process of rescanning.
   */
  public static boolean isRescanning()
  {
    return rescanInProgress.get();
  }

  /**
   * Asynchronously loads the game file cache from disk, without checking
   * if the games are still present in the user's configured folders.
   * If this has already been called, calling it again has no effect.
   */
  public static void startLoad(Context context)
  {
    if (loadInProgress.compareAndSet(false, true))
    {
      new AfterDirectoryInitializationRunner().run(context, false,
              () -> executor.execute(GameFileCacheManager::load));
    }
  }

  /**
   * Asynchronously scans for games in the user's configured folders,
   * updating the game file cache with the results.
   * If startLoad hasn't been called before this, this has no effect.
   */
  public static void startRescan(Context context)
  {
    if (rescanInProgress.compareAndSet(false, true))
    {
      new AfterDirectoryInitializationRunner().run(context, false,
              () -> executor.execute(GameFileCacheManager::rescan));
    }
  }

  public static GameFile addOrGet(String gamePath)
  {
    // Common case: The game is in the cache, so just grab it from there.
    // (Actually, addOrGet already checks for this case, but we want to avoid calling it if possible
    // because onHandleIntent may hold a lock on gameFileCache for extended periods of time.)
    GameFile[] allGames = gameFiles.get();
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
          sendBroadcast(CACHE_UPDATED);
        }
      }
    }

    loadInProgress.set(false);
    if (!rescanInProgress.get())
      sendBroadcast(DONE_LOADING);
  }

  /**
   * Scans for games in the user's configured folders,
   * updating the game file cache with the results.
   * If load hasn't been called before this, this has no effect.
   */
  private static void rescan()
  {
    if (gameFileCache != null)
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
        sendBroadcast(CACHE_UPDATED);
      }

      boolean additionalMetadataChanged = gameFileCache.updateAdditionalMetadata();
      if (additionalMetadataChanged)
      {
        updateGameFileArray();
        sendBroadcast(CACHE_UPDATED);
      }

      if (changed || additionalMetadataChanged)
      {
        gameFileCache.save();
      }
    }

    rescanInProgress.set(false);
    if (!loadInProgress.get())
      sendBroadcast(DONE_LOADING);
  }

  private static void updateGameFileArray()
  {
    GameFile[] gameFilesTemp = gameFileCache.getAllGames();
    Arrays.sort(gameFilesTemp, (lhs, rhs) -> lhs.getTitle().compareToIgnoreCase(rhs.getTitle()));
    gameFiles.set(gameFilesTemp);
  }

  private static void sendBroadcast(String action)
  {
    LocalBroadcastManager.getInstance(DolphinApplication.getAppContext())
            .sendBroadcast(new Intent(action));
  }
}
