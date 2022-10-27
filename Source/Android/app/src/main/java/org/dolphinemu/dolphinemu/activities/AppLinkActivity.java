// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.fragment.app.FragmentActivity;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.main.MainActivity;
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
    mAfterDirectoryInitializationRunner.runWithLifecycle(this, () -> tryPlay(playAction));

    GameFileCacheManager.isLoading().observe(this, (isLoading) ->
    {
      if (!isLoading && DirectoryInitialization.areDolphinDirectoriesReady())
      {
        tryPlay(playAction);
      }
    });

    DirectoryInitialization.start(this);
    GameFileCacheManager.startLoad();
  }

  /**
   * Action if channel icon is selected
   */
  private void browse()
  {
    Intent openApp = new Intent(this, MainActivity.class);
    startActivity(openApp);

    finish();
  }

  private void tryPlay(AppLinkHelper.PlayAction action)
  {
    // TODO: This approach of getting the game from the game file cache without rescanning the
    //       library means that we can fail to launch games if the cache file has been deleted.

    GameFile game = GameFileCacheManager.getGameFileByGameId(action.getGameId());

    // If game == null and the load isn't done, wait for the next GameFileCacheService broadcast.
    // If game == null and the load is done, call play with a null game, making us exit in failure.
    if (game != null || !GameFileCacheManager.isLoading().getValue())
    {
      play(game);
    }
  }

  /**
   * Action if program(game) is selected
   */
  private void play(GameFile game)
  {
    // Check if game is valid
    try
    {
      game.getGameId();
    }
    catch (NullPointerException e)
    {
      Toast.makeText(this, R.string.game_not_found, Toast.LENGTH_SHORT).show();
      finish();
      return;
    }
    startGame(game);
    finish();
  }

  private void startGame(GameFile game)
  {
    mAfterDirectoryInitializationRunner.cancel();
    EmulationActivity.launch(this, GameFileCacheManager.findSecondDiscAndGetPaths(game), false);
  }
}
