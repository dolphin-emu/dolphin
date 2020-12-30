package org.dolphinemu.dolphinemu.ui.main;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.ContentHandler;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;

import java.util.Arrays;
import java.util.Set;

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
      case R.id.menu_settings:
        mView.launchSettingsActivity(MenuTag.SETTINGS);
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
        new AfterDirectoryInitializationRunner().run(context, true, mView::launchInstallWAD);
        return true;
    }

    return false;
  }

  public void addDirIfNeeded()
  {
    if (mDirToAdd != null)
    {
      GameFileCache.addGameFolder(mDirToAdd);
      mDirToAdd = null;
    }
  }

  public void onDirectorySelected(Intent result)
  {
    Uri uri = result.getData();

    boolean recursive = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBooleanGlobal();
    String[] childNames = ContentHandler.getChildNames(uri, recursive);
    if (Arrays.stream(childNames).noneMatch((name) -> FileBrowserHelper.GAME_EXTENSIONS.contains(
            FileBrowserHelper.getExtension(name, false))))
    {
      AlertDialog.Builder builder = new AlertDialog.Builder(mContext, R.style.DolphinDialogBase);
      builder.setMessage(mContext.getString(R.string.wrong_file_extension_in_directory,
              FileBrowserHelper.setToSortedDelimitedString(FileBrowserHelper.GAME_EXTENSIONS)));
      builder.setPositiveButton(R.string.ok, null);
      builder.show();
    }

    ContentResolver contentResolver = mContext.getContentResolver();
    Uri canonicalizedUri = contentResolver.canonicalize(uri);
    if (canonicalizedUri != null)
      uri = canonicalizedUri;

    int takeFlags = result.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
    mContext.getContentResolver().takePersistableUriPermission(uri, takeFlags);

    mDirToAdd = uri.toString();
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
