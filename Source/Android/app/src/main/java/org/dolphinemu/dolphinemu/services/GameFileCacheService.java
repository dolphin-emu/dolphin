package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.support.v4.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A service that loads game list data on a separate thread.
 */
public final class GameFileCacheService extends IntentService
{
  public static final String BROADCAST_ACTION = "org.dolphinemu.dolphinemu.GAME_FILE_CACHE_UPDATED";

  private static final String ACTION_LOAD = "org.dolphinemu.dolphinemu.LOAD_GAME_FILE_CACHE";
  private static final String ACTION_RESCAN = "org.dolphinemu.dolphinemu.RESCAN_GAME_FILE_CACHE";

  private static GameFileCache gameFileCache = null;
  private static AtomicReference<GameFile[]> gameFiles = new AtomicReference<>(new GameFile[]{});
  private static AtomicBoolean hasLoadedCache = new AtomicBoolean(false);
  private static AtomicBoolean hasScannedLibrary = new AtomicBoolean(false);

  public GameFileCacheService()
  {
    // Superclass constructor is called to name the thread on which this service executes.
    super("GameFileCacheService");
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

  public static boolean hasLoadedCache()
  {
    return hasLoadedCache.get();
  }

  public static boolean hasScannedLibrary()
  {
    return hasScannedLibrary.get();
  }

  private static void startService(Context context, String action)
  {
    Intent intent = new Intent(context, GameFileCacheService.class);
    intent.setAction(action);
    context.startService(intent);
  }

  /**
   * Asynchronously loads the game file cache from disk without checking
   * which games are present on the file system.
   */
  public static void startLoad(Context context)
  {
    new AfterDirectoryInitializationRunner().run(context,
            () -> startService(context, ACTION_LOAD));
  }

  /**
   * Asynchronously scans for games in the user's configured folders,
   * updating the game file cache with the results.
   * If startLoad hasn't been called before this, this has no effect.
   */
  public static void startRescan(Context context)
  {
    new AfterDirectoryInitializationRunner().run(context,
            () -> startService(context, ACTION_RESCAN));
  }

  public static GameFile addOrGet(String gamePath)
  {
    // The existence of this one function, which is called from one
    // single place, forces us to use synchronization in onHandleIntent...
    // A bit annoying, but should be good enough for now
    synchronized (gameFileCache)
    {
      return gameFileCache.addOrGet(gamePath);
    }
  }

  @Override
  protected void onHandleIntent(Intent intent)
  {
    // Load the game list cache if it isn't already loaded, otherwise do nothing
    if (ACTION_LOAD.equals(intent.getAction()) && gameFileCache == null)
    {
      GameFileCache temp = new GameFileCache(getCacheDir() + File.separator + "gamelist.cache");
      synchronized (temp)
      {
        gameFileCache = temp;
        gameFileCache.load();
        updateGameFileArray();
        hasLoadedCache.set(true);
        sendBroadcast();
      }
    }

    // Rescan the file system and update the game list cache with the results
    if (ACTION_RESCAN.equals(intent.getAction()) && gameFileCache != null)
    {
      synchronized (gameFileCache)
      {
        boolean changed = gameFileCache.scanLibrary(this);
        if (changed)
          updateGameFileArray();
        hasScannedLibrary.set(true);
        sendBroadcast();
      }
    }
  }

  private void updateGameFileArray()
  {
    GameFile[] gameFilesTemp = gameFileCache.getAllGames();
    Arrays.sort(gameFilesTemp, (lhs, rhs) -> lhs.getTitle().compareToIgnoreCase(rhs.getTitle()));
    gameFiles.set(gameFilesTemp);
  }

  private void sendBroadcast()
  {
    LocalBroadcastManager.getInstance(this).sendBroadcast(new Intent(BROADCAST_ACTION));
  }
}
