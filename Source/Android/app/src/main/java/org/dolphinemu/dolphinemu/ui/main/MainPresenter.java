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
    runOnThreadAndShowResult(R.string.import_in_progress, () ->
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

    runOnThreadAndShowResult(R.string.import_in_progress, () ->
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

  private void runOnThreadAndShowResult(int progressMessage, Supplier<String> f)
  {
    final Activity mainPresenterActivity = (Activity) mContext;

    AlertDialog progressDialog = new AlertDialog.Builder(mContext, R.style.DolphinDialogBase)
            .create();
    progressDialog.setTitle(progressMessage);
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
    }, mContext.getResources().getString(progressMessage)).start();
  }
}
