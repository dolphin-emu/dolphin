// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import androidx.fragment.app.FragmentActivity;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.ui.main.TvMainActivity;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.AppLinkHelper;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

/**
 * Linker between leanback homescreen and app
 */
public class AppLinkActivity extends FragmentActivity
{
  private static final String TAG = "AppLinkActivity";

  private AppLinkHelper.PlayAction playAction;
  private AfterDirectoryInitializationRunner mAfterDirectoryInitializationRunner;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    Intent intent = getIntent();
    Uri uri = intent.getData();

    Log.v(TAG, uri.toString());

    if (uri.getPathSegments().isEmpty())
    {
      Log.e(TAG, "Invalid uri " + uri);
      finish();
      return;
    }

    AppLinkHelper.AppLinkAction action = AppLinkHelper.extractAction(uri);
    switch (action.getAction())
    {
      case AppLinkHelper.PLAY:
        playAction = (AppLinkHelper.PlayAction) action;
        initResources();
        break;
      case AppLinkHelper.BROWSE:
        browse();
        break;
      default:
        throw new IllegalArgumentException("Invalid Action " + action);
    }
  }

  /**
   * Need to init these since they usually occur in the main activity.
   */
  private void initResources()
  {
    mAfterDirectoryInitializationRunner = new AfterDirectoryInitializationRunner();
    mAfterDirectoryInitializationRunner.run(this, true, () -> tryPlay(playAction));

    IntentFilter gameFileCacheIntentFilter = new IntentFilter(GameFileCacheService.DONE_LOADING);

    BroadcastReceiver gameFileCacheReceiver = new BroadcastReceiver()
    {
      @Override
      public void onReceive(Context context, Intent intent)
      {
        if (DirectoryInitialization.areDolphinDirectoriesReady())
        {
          tryPlay(playAction);
        }
      }
    };

    LocalBroadcastManager broadcastManager = LocalBroadcastManager.getInstance(this);
    broadcastManager.registerReceiver(gameFileCacheReceiver, gameFileCacheIntentFilter);

    DirectoryInitialization.start(this);
    GameFileCacheService.startLoad(this);
  }

  /**
   * Action if channel icon is selected
   */
  private void browse()
  {
    Intent openApp = new Intent(this, TvMainActivity.class);
    startActivity(openApp);

    finish();
  }

  private void tryPlay(AppLinkHelper.PlayAction action)
  {
    // TODO: This approach of getting the game from the game file cache without rescanning the
    //       library means that we can fail to launch games if the cache file has been deleted.

    GameFile game = GameFileCacheService.getGameFileByGameId(action.getGameId());

    // If game == null and the load isn't done, wait for the next GameFileCacheService broadcast.
    // If game == null and the load is done, call play with a null game, making us exit in failure.
    if (game != null || !GameFileCacheService.isLoading())
    {
      play(action, game);
    }
  }

  /**
   * Action if program(game) is selected
   */
  private void play(AppLinkHelper.PlayAction action, GameFile game)
  {
    Log.d(TAG, "Playing game "
            + action.getGameId()
            + " from channel "
            + action.getChannelId());

    if (game == null)
      Log.e(TAG, "Invalid Game: " + action.getGameId());
    else
      startGame(game);
    finish();
  }

  private void startGame(GameFile game)
  {
    if (mAfterDirectoryInitializationRunner != null)
    {
      mAfterDirectoryInitializationRunner.cancel();
      mAfterDirectoryInitializationRunner = null;
    }
    EmulationActivity.launch(this, GameFileCacheService.findSecondDiscAndGetPaths(game));
  }
}
