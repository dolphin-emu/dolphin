package org.dolphinemu.dolphinemu.activities;

import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.ui.main.TvMainActivity;
import org.dolphinemu.dolphinemu.utils.AppLinkHelper;
import org.dolphinemu.dolphinemu.utils.DirectoryStateReceiver;

/**
 * Linker between leanback homescreen and app
 */
public class AppLinkActivity extends FragmentActivity
{
  private static final String TAG = "AppLinkActivity";

  private AppLinkHelper.PlayAction playAction;
  private DirectoryStateReceiver directoryStateReceiver;

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
    IntentFilter statusIntentFilter = new IntentFilter(
            DirectoryInitialization.BROADCAST_ACTION);

    directoryStateReceiver =
            new DirectoryStateReceiver(directoryInitializationState ->
            {
              if (directoryInitializationState ==
                      DirectoryInitialization.DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
              {
                play(playAction);
              }
              else if (directoryInitializationState ==
                      DirectoryInitialization.DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED)
              {
                Toast.makeText(this, R.string.write_permission_needed, Toast.LENGTH_SHORT)
                        .show();
              }
              else if (directoryInitializationState ==
                      DirectoryInitialization.DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE)
              {
                Toast.makeText(this, R.string.external_storage_not_mounted, Toast.LENGTH_SHORT)
                        .show();
              }
            });

    // Registers the DirectoryStateReceiver and its intent filters
    LocalBroadcastManager.getInstance(this).registerReceiver(
            directoryStateReceiver,
            statusIntentFilter);
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

  /**
   * Action if program(game) is selected
   */
  private void play(AppLinkHelper.PlayAction action)
  {
    Log.d(TAG, "Playing game "
            + action.getGameId()
            + " from channel "
            + action.getChannelId());

    GameFile game = GameFileCacheService.getGameFileByGameId(action.getGameId());
    if (game == null)
      Log.e(TAG, "Invalid Game: " + action.getGameId());
    else
      startGame(game);
    finish();
  }

  private void startGame(GameFile game)
  {
    if (directoryStateReceiver != null)
    {
      LocalBroadcastManager.getInstance(this).unregisterReceiver(directoryStateReceiver);
      directoryStateReceiver = null;
    }
    EmulationActivity.launch(this, game);
  }
}
