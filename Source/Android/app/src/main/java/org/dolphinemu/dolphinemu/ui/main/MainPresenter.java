package org.dolphinemu.dolphinemu.ui.main;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.support.v4.content.LocalBroadcastManager;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

import java.io.File;

public final class MainPresenter
{
  public static final int REQUEST_ADD_DIRECTORY = 1;
  public static final int REQUEST_EMULATE_GAME = 2;

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
    String versionName = "5.0(MMJ)";//BuildConfig.VERSION_NAME;
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
      case R.id.menu_add_directory:
        mView.launchFileListActivity();
        return true;

      case R.id.menu_settings_core:
        mView.launchSettingsActivity(MenuTag.CONFIG);
        return true;

      case R.id.menu_settings_gcpad:
        mView.launchSettingsActivity(MenuTag.GCPAD_TYPE);
        return true;

      case R.id.menu_settings_wiimote:
        mView.launchSettingsActivity(MenuTag.WIIMOTE);
        return true;

      case R.id.menu_clear_data:
        clearGameData(context);
        break;

      case R.id.menu_refresh:
        GameFileCacheService.startRescan(context);
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

  public void refreshFragmentScreenshot(int resultCode)
  {
    mView.refreshFragmentScreenshot(resultCode);
  }

  private void clearGameData(Context context)
  {
    int count = 0;
    String cachePath = DirectoryInitialization.getCacheDirectory();
    File dir = new File(cachePath);
    if (dir.exists())
    {
      for (File f : dir.listFiles())
      {
        if (f.getName().endsWith(".uidcache"))
        {
          if (f.delete())
          {
            count += 1;
          }
        }
      }
    }

    String shadersPath = cachePath + File.separator + "Shaders";
    dir = new File(shadersPath);
    if (dir.exists())
    {
      for (File f : dir.listFiles())
      {
        if (f.getName().endsWith(".cache"))
        {
          if (f.delete())
          {
            count += 1;
          }
        }
      }
    }

    SharedPreferences pref = PreferenceManager.getDefaultSharedPreferences(context);
    SharedPreferences.Editor editor = pref.edit();
    editor.putBoolean(InputOverlay.CONTROL_INIT_PREF_KEY, false);
    editor.putInt(InputOverlay.CONTROL_TYPE_PREF_KEY, 2);
    editor.apply();

    Toast.makeText(context, String.format("Delete %d files", count), Toast.LENGTH_SHORT).show();
  }
}
