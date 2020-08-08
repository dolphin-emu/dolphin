package org.dolphinemu.dolphinemu.ui.main;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;

public final class MainPresenter
{
  public static final int REQUEST_DIRECTORY = 1;
  public static final int REQUEST_GAME_FILE = 2;
  public static final int REQUEST_SD_FILE = 3;
  public static final int REQUEST_WAD_FILE = 4;

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

      case R.id.menu_install_wad:
        mView.launchInstallWAD();
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
    }
  }

  public void onDirectorySelected(String dir)
  {
    mDirToAdd = dir;
  }

  public void installWAD(String file)
  {
    final Activity mainPresenterActivity = (Activity) mContext;

    AlertDialog dialog = new AlertDialog.Builder(mContext, R.style.DolphinDialogBase).create();
    dialog.setTitle("Installing WAD");
    dialog.setMessage("Installing...");
    dialog.setCancelable(false);
    dialog.show();

    Thread installWADThread = new Thread(() ->
    {
      if (NativeLibrary.InstallWAD(file))
      {
        mainPresenterActivity.runOnUiThread(
                () -> Toast.makeText(mContext, R.string.wad_install_success, Toast.LENGTH_SHORT)
                        .show());
      }
      else
      {
        mainPresenterActivity.runOnUiThread(
                () -> Toast.makeText(mContext, R.string.wad_install_failure, Toast.LENGTH_SHORT)
                        .show());
      }
      mainPresenterActivity.runOnUiThread(dialog::dismiss);
    }, "InstallWAD");
    installWADThread.start();
  }
}
