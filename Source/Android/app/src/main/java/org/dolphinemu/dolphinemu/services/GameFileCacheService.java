package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.support.v4.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.model.GameFileCache;

import java.util.Arrays;
import java.util.List;
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

  public GameFileCacheService()
  {
    // Superclass constructor is called to name the thread on which this service executes.
    super("GameFileCacheService");
  }

  public static List<GameFile> getAllGameFiles()
  {
    return Arrays.asList(gameFiles.get());
  }

  public static GameFile getGameFileByPath(String path)
  {
    for(GameFile game : gameFiles.get())
    {
      if(path.equals(game.getPath()))
      {
        return game;
      }
    }
    return null;
  }

  public static String[] getAllDiscPaths(GameFile game)
  {
    String[] paths = new String[2];
    GameFile matchWithoutRevision = null;
    paths[0] = game.getPath();

    for (GameFile otherGame : gameFiles.get())
    {
      if (game.getGameId().equals(otherGame.getGameId()) &&
              game.getDiscNumber() != otherGame.getDiscNumber())
      {
        if (game.getRevision() == otherGame.getRevision())
        {
          paths[1] = otherGame.getPath();
          return paths;
        }
        else
        {
          matchWithoutRevision = otherGame;
        }
      }
    }

    if (matchWithoutRevision != null)
      paths[1] = matchWithoutRevision.getPath();

    return paths;
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
    startService(context, ACTION_LOAD);
  }

  /**
   * Asynchronously scans for games in the user's configured folders,
   * updating the game file cache with the results.
   * If startLoad hasn't been called before this, this has no effect.
   */
  public static void startRescan(Context context)
  {
    startService(context, ACTION_RESCAN);
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
    if (ACTION_LOAD.equals(intent.getAction()))
    {
      if (gameFileCache != null)
        return;

      // Load the game list cache if it isn't already loaded, otherwise do nothing
      GameFileCache temp = new GameFileCache();
      synchronized (temp)
      {
        gameFileCache = temp;
        gameFileCache.load();
        updateGameFileArray();
      }
    }
    else if (ACTION_RESCAN.equals(intent.getAction()))
    {
      if (gameFileCache == null)
        return;

      // Rescan the file system and update the game list cache with the results
      synchronized (gameFileCache)
      {
        if (gameFileCache.scanLibrary(this))
        {
          updateGameFileArray();
        }
      }
    }
  }

  private void updateGameFileArray()
  {
    GameFile[] gameFilesTemp = gameFileCache.getAllGames();
    Arrays.sort(gameFilesTemp, (lhs, rhs) -> {
        int ret = lhs.getGameId().compareToIgnoreCase(rhs.getGameId());
        return ret == 0 ? Integer.compare(lhs.getDiscNumber(), rhs.getDiscNumber()) : ret;
    });
    gameFiles.set(gameFilesTemp);
    LocalBroadcastManager.getInstance(this).sendBroadcast(new Intent(BROADCAST_ACTION));
  }
}
