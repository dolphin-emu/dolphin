package org.dolphinemu.dolphinemu.ui.main;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.support.v4.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;

public final class MainPresenter
{
  public static final int REQUEST_ADD_DIRECTORY = 1;
  public static final int REQUEST_OPEN_FILE = 2;

  private final MainView mView;
  private final Context mContext;
  private BroadcastReceiver mBroadcastReceiver = null;
  private String mDirToAdd;

  public MainPresenter(MainView view, Context context)
  {
    mView = view;
    mContext = context;
  }

  public void onCreate()
  {
    String versionName = BuildConfig.VERSION_NAME;
    mView.setVersionString(versionName);

    IntentFilter filter = new IntentFilter();
    filter.addAction(GameFileCacheService.BROADCAST_ACTION);
    mBroadcastReceiver = new BroadcastReceiver()
    {
      @Override
      public void onReceive(Context context, Intent intent)
      {
        mView.showGames();
      }
    };
    LocalBroadcastManager.getInstance(mContext).registerReceiver(mBroadcastReceiver, filter);
  }

  public void onDestroy()
  {
    if (mBroadcastReceiver != null)
    {
      LocalBroadcastManager.getInstance(mContext).unregisterReceiver(mBroadcastReceiver);
    }
  }

  public void onFabClick()
  {
    mView.launchFileListActivity();
  }

  public boolean handleOptionSelection(int itemId, Context context)
  {
    switch (itemId)
    {
      case R.id.menu_settings_core:
        mView.launchSettingsActivity(MenuTag.CONFIG);
        return true;

      case R.id.menu_settings_graphics:
        mView.launchSettingsActivity(MenuTag.GRAPHICS);
        return true;

      case R.id.menu_settings_gcpad:
        mView.launchSettingsActivity(MenuTag.GCPAD_TYPE);
        return true;

      case R.id.menu_settings_wiimote:
        mView.launchSettingsActivity(MenuTag.WIIMOTE);
        return true;

      case R.id.menu_refresh:
        GameFileCacheService.startRescan(context);
        return true;

      case R.id.button_add_directory:
        mView.launchFileListActivity();
        return true;

      case R.id.menu_open_file:
        mView.launchOpenFileActivity();
        return true;
    }

    return false;
  }

  public void addDirIfNeeded(Context context)
  {
    if (mDirToAdd != null)
    {
      GameFileCache.addGameFolder(mDirToAdd, context);
      mDirToAdd = null;
      GameFileCacheService.startRescan(context);
    }
  }

  public void onDirectorySelected(String dir)
  {
    mDirToAdd = dir;
  }
}
