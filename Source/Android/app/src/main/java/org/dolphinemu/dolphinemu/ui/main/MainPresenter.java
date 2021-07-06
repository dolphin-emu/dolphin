// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;

import androidx.appcompat.app.AlertDialog;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.BooleanSupplier;
import org.dolphinemu.dolphinemu.utils.CompletableFuture;
import org.dolphinemu.dolphinemu.utils.ContentHandler;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.WiiUtils;

import java.util.Arrays;
import java.util.concurrent.ExecutionException;
import java.util.function.Supplier;

public final class MainPresenter
{
  public static final int REQUEST_DIRECTORY = 1;
  public static final int REQUEST_GAME_FILE = 2;
  public static final int REQUEST_SD_FILE = 3;
  public static final int REQUEST_WAD_FILE = 4;
  public static final int REQUEST_WII_SAVE_FILE = 5;
  public static final int REQUEST_NAND_BIN_FILE = 6;

  private static boolean sShouldRescanLibrary = true;

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
    filter.addAction(GameFileCacheService.CACHE_UPDATED);
    filter.addAction(GameFileCacheService.DONE_LOADING);
    mBroadcastReceiver = new BroadcastReceiver()
    {
      @Override
      public void onReceive(Context context, Intent intent)
      {
        switch (intent.getAction())
        {
          case GameFileCacheService.CACHE_UPDATED:
            mView.showGames();
            break;
          case GameFileCacheService.DONE_LOADING:
            mView.setRefreshing(false);
            break;
        }
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
        mView.setRefreshing(true);
        GameFileCacheService.startRescan(context);
        return true;

      case R.id.button_add_directory:
        mView.launchFileListActivity();
        return true;

      case R.id.menu_open_file:
        mView.launchOpenFileActivity(REQUEST_GAME_FILE);
        return true;

      case R.id.menu_install_wad:
        new AfterDirectoryInitializationRunner().run(context, true,
                () -> mView.launchOpenFileActivity(REQUEST_WAD_FILE));
        return true;

      case R.id.menu_import_wii_save:
        new AfterDirectoryInitializationRunner().run(context, true,
                () -> mView.launchOpenFileActivity(REQUEST_WII_SAVE_FILE));
        return true;

      case R.id.menu_import_nand_backup:
        new AfterDirectoryInitializationRunner().run(context, true,
                () -> mView.launchOpenFileActivity(REQUEST_NAND_BIN_FILE));
        return true;
    }

    return false;
  }

  public void onResume()
  {
    if (mDirToAdd != null)
    {
      GameFileCache.addGameFolder(mDirToAdd);
      mDirToAdd = null;
    }

    if (sShouldRescanLibrary && !GameFileCacheService.isRescanning())
    {
      new AfterDirectoryInitializationRunner().run(mContext, false, () ->
      {
        mView.setRefreshing(true);
        GameFileCacheService.startRescan(mContext);
      });
    }

    sShouldRescanLibrary = true;
  }

  /**
   * Called when a selection is made using the legacy folder picker.
   */
  public void onDirectorySelected(String dir)
  {
    mDirToAdd = dir;
  }

  /**
   * Called when a selection is made using the Storage Access Framework folder picker.
   */
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

  public void installWAD(String path)
  {
    runOnThreadAndShowResult(R.string.import_in_progress, 0, () ->
    {
      boolean success = WiiUtils.installWAD(path);
      int message = success ? R.string.wad_install_success : R.string.wad_install_failure;
      return mContext.getResources().getString(message);
    });
  }

  public void importWiiSave(String path)
  {
    final Activity mainPresenterActivity = (Activity) mContext;

    CompletableFuture<Boolean> canOverwriteFuture = new CompletableFuture<>();

    runOnThreadAndShowResult(R.string.import_in_progress, 0, () ->
    {
      BooleanSupplier canOverwrite = () ->
      {
        mainPresenterActivity.runOnUiThread(() ->
        {
          AlertDialog.Builder builder =
                  new AlertDialog.Builder(mContext, R.style.DolphinDialogBase);
          builder.setMessage(R.string.wii_save_exists);
          builder.setCancelable(false);
          builder.setPositiveButton(R.string.yes, (dialog, i) -> canOverwriteFuture.complete(true));
          builder.setNegativeButton(R.string.no, (dialog, i) -> canOverwriteFuture.complete(false));
          builder.show();
        });

        try
        {
          return canOverwriteFuture.get();
        }
        catch (ExecutionException | InterruptedException e)
        {
          // Shouldn't happen
          throw new RuntimeException(e);
        }
      };

      int result = WiiUtils.importWiiSave(path, canOverwrite);

      int message;
      switch (result)
      {
        case WiiUtils.RESULT_SUCCESS:
          message = R.string.wii_save_import_success;
          break;
        case WiiUtils.RESULT_CORRUPTED_SOURCE:
          message = R.string.wii_save_import_corruped_source;
          break;
        case WiiUtils.RESULT_TITLE_MISSING:
          message = R.string.wii_save_import_title_missing;
          break;
        case WiiUtils.RESULT_CANCELLED:
          return null;
        default:
          message = R.string.wii_save_import_error;
          break;
      }
      return mContext.getResources().getString(message);
    });
  }

  public void importNANDBin(String path)
  {
    AlertDialog.Builder builder =
            new AlertDialog.Builder(mContext, R.style.DolphinDialogBase);

    builder.setMessage(R.string.nand_import_warning);
    builder.setNegativeButton(R.string.no, (dialog, i) -> dialog.dismiss());
    builder.setPositiveButton(R.string.yes, (dialog, i) ->
    {
      dialog.dismiss();

      runOnThreadAndShowResult(R.string.import_in_progress, R.string.do_not_close_app, () ->
      {
        // ImportNANDBin doesn't provide any result value, unfortunately...
        // It does however show a panic alert if something goes wrong.
        WiiUtils.importNANDBin(path);
        return null;
      });
    });

    builder.show();
  }

  private void runOnThreadAndShowResult(int progressTitle, int progressMessage, Supplier<String> f)
  {
    final Activity mainPresenterActivity = (Activity) mContext;

    AlertDialog progressDialog = new AlertDialog.Builder(mContext, R.style.DolphinDialogBase)
            .create();
    progressDialog.setTitle(progressTitle);
    if (progressMessage != 0)
      progressDialog.setMessage(mContext.getResources().getString(progressMessage));
    progressDialog.setCancelable(false);
    progressDialog.show();

    new Thread(() ->
    {
      String result = f.get();
      mainPresenterActivity.runOnUiThread(() ->
      {
        progressDialog.dismiss();

        if (result != null)
        {
          AlertDialog.Builder builder =
                  new AlertDialog.Builder(mContext, R.style.DolphinDialogBase);
          builder.setMessage(result);
          builder.setPositiveButton(R.string.ok, (dialog, i) -> dialog.dismiss());
          builder.show();
        }
      });
    }, mContext.getResources().getString(progressTitle)).start();
  }

  public static void skipRescanningLibrary()
  {
    sShouldRescanLibrary = false;
  }
}
