// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main;

import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;

import androidx.activity.ComponentActivity;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemUpdateProgressBarDialogFragment;
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemMenuNotInstalledDialogFragment;
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemUpdateViewModel;
import org.dolphinemu.dolphinemu.fragments.AboutDialogFragment;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.BooleanSupplier;
import org.dolphinemu.dolphinemu.utils.CompletableFuture;
import org.dolphinemu.dolphinemu.utils.ContentHandler;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.ThreadUtil;
import org.dolphinemu.dolphinemu.utils.WiiUtils;

import java.util.Arrays;
import java.util.concurrent.ExecutionException;

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
  private final FragmentActivity mActivity;
  private String mDirToAdd;

  public MainPresenter(MainView view, FragmentActivity activity)
  {
    mView = view;
    mActivity = activity;
  }

  public void onCreate()
  {
    // Ask the user to grant write permission if relevant and not already granted
    if (DirectoryInitialization.isWaitingForWriteAccess(mActivity))
      PermissionsHandler.requestWritePermission(mActivity);

    String versionName = BuildConfig.VERSION_NAME;
    mView.setVersionString(versionName);

    GameFileCacheManager.getGameFiles().observe(mActivity, (gameFiles) -> mView.showGames());

    Observer<Boolean> refreshObserver = (isLoading) ->
    {
      mView.setRefreshing(GameFileCacheManager.isLoadingOrRescanning());
    };
    GameFileCacheManager.isLoading().observe(mActivity, refreshObserver);
    GameFileCacheManager.isRescanning().observe(mActivity, refreshObserver);
  }

  public void onDestroy()
  {
  }

  public void onFabClick()
  {
    new AfterDirectoryInitializationRunner().runWithLifecycle(mActivity,
            mView::launchFileListActivity);
  }

  public boolean handleOptionSelection(int itemId, ComponentActivity activity)
  {
    switch (itemId)
    {
      case R.id.menu_settings:
        mView.launchSettingsActivity(MenuTag.SETTINGS);
        return true;

      case R.id.menu_grid_options:
        mView.showGridOptions();
        return true;

      case R.id.menu_refresh:
        mView.setRefreshing(true);
        GameFileCacheManager.startRescan();
        return true;

      case R.id.button_add_directory:
        new AfterDirectoryInitializationRunner().runWithLifecycle(activity,
                mView::launchFileListActivity);
        return true;

      case R.id.menu_open_file:
        mView.launchOpenFileActivity(REQUEST_GAME_FILE);
        return true;

      case R.id.menu_load_wii_system_menu:
        launchWiiSystemMenu();
        return true;

      case R.id.menu_online_system_update:
        new AfterDirectoryInitializationRunner().runWithLifecycle(activity,
                this::launchOnlineUpdate);
        return true;

      case R.id.menu_install_wad:
        new AfterDirectoryInitializationRunner().runWithLifecycle(activity,
                () -> mView.launchOpenFileActivity(REQUEST_WAD_FILE));
        return true;

      case R.id.menu_import_wii_save:
        new AfterDirectoryInitializationRunner().runWithLifecycle(activity,
                () -> mView.launchOpenFileActivity(REQUEST_WII_SAVE_FILE));
        return true;

      case R.id.menu_import_nand_backup:
        new AfterDirectoryInitializationRunner().runWithLifecycle(activity,
                () -> mView.launchOpenFileActivity(REQUEST_NAND_BIN_FILE));
        return true;

      case R.id.menu_about:
        showAboutDialog();
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

    if (sShouldRescanLibrary)
    {
      GameFileCacheManager.startRescan();
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

    boolean recursive = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBoolean();
    String[] childNames = ContentHandler.getChildNames(uri, recursive);
    if (Arrays.stream(childNames).noneMatch((name) -> FileBrowserHelper.GAME_EXTENSIONS.contains(
            FileBrowserHelper.getExtension(name, false))))
    {
      new MaterialAlertDialogBuilder(mActivity)
              .setMessage(mActivity.getString(R.string.wrong_file_extension_in_directory,
                      FileBrowserHelper.setToSortedDelimitedString(
                              FileBrowserHelper.GAME_EXTENSIONS)))
              .setPositiveButton(R.string.ok, null)
              .show();
    }

    ContentResolver contentResolver = mActivity.getContentResolver();
    Uri canonicalizedUri = contentResolver.canonicalize(uri);
    if (canonicalizedUri != null)
      uri = canonicalizedUri;

    int takeFlags = result.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
    mActivity.getContentResolver().takePersistableUriPermission(uri, takeFlags);

    mDirToAdd = uri.toString();
  }

  public void installWAD(String path)
  {
    ThreadUtil.runOnThreadAndShowResult(mActivity, R.string.import_in_progress, 0, () ->
    {
      boolean success = WiiUtils.installWAD(path);
      int message = success ? R.string.wad_install_success : R.string.wad_install_failure;
      return mActivity.getResources().getString(message);
    });
  }

  public void importWiiSave(String path)
  {
    CompletableFuture<Boolean> canOverwriteFuture = new CompletableFuture<>();

    ThreadUtil.runOnThreadAndShowResult(mActivity, R.string.import_in_progress, 0, () ->
    {
      BooleanSupplier canOverwrite = () ->
      {
        mActivity.runOnUiThread(() ->
        {
          new MaterialAlertDialogBuilder(mActivity)
                  .setMessage(R.string.wii_save_exists)
                  .setCancelable(false)
                  .setPositiveButton(R.string.yes, (dialog, i) -> canOverwriteFuture.complete(true))
                  .setNegativeButton(R.string.no, (dialog, i) -> canOverwriteFuture.complete(false))
                  .show();
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
      return mActivity.getResources().getString(message);
    });
  }

  public void importNANDBin(String path)
  {
    new MaterialAlertDialogBuilder(mActivity)
            .setMessage(R.string.nand_import_warning)
            .setNegativeButton(R.string.no, (dialog, i) -> dialog.dismiss())
            .setPositiveButton(R.string.yes, (dialog, i) ->
            {
              dialog.dismiss();

              ThreadUtil.runOnThreadAndShowResult(mActivity, R.string.import_in_progress,
                      R.string.do_not_close_app, () ->
                      {
                        // ImportNANDBin unfortunately doesn't provide any result value...
                        // It does however show a panic alert if something goes wrong.
                        WiiUtils.importNANDBin(path);
                        return null;
                      });
            })
            .show();
  }

  public static void skipRescanningLibrary()
  {
    sShouldRescanLibrary = false;
  }

  private void launchOnlineUpdate()
  {
    if (WiiUtils.isSystemMenuInstalled())
    {
      SystemUpdateViewModel viewModel =
              new ViewModelProvider(mActivity).get(SystemUpdateViewModel.class);
      viewModel.setRegion(-1);
      launchUpdateProgressBarFragment(mActivity);
    }
    else
    {
      SystemMenuNotInstalledDialogFragment dialogFragment =
              new SystemMenuNotInstalledDialogFragment();
      dialogFragment
              .show(mActivity.getSupportFragmentManager(), "SystemMenuNotInstalledDialogFragment");
    }
  }

  public static void launchDiscUpdate(String path, FragmentActivity activity)
  {
    SystemUpdateViewModel viewModel =
            new ViewModelProvider(activity).get(SystemUpdateViewModel.class);
    viewModel.setDiscPath(path);
    launchUpdateProgressBarFragment(activity);
  }

  private static void launchUpdateProgressBarFragment(FragmentActivity activity)
  {
    SystemUpdateProgressBarDialogFragment progressBarFragment =
            new SystemUpdateProgressBarDialogFragment();
    progressBarFragment
            .show(activity.getSupportFragmentManager(), SystemUpdateProgressBarDialogFragment.TAG);
    progressBarFragment.setCancelable(false);
  }

  private void launchWiiSystemMenu()
  {
    new AfterDirectoryInitializationRunner().runWithLifecycle(mActivity, () ->
    {
      if (WiiUtils.isSystemMenuInstalled())
      {
        EmulationActivity.launchSystemMenu(mActivity);
      }
      else
      {
        SystemMenuNotInstalledDialogFragment dialogFragment =
                new SystemMenuNotInstalledDialogFragment();
        dialogFragment
                .show(mActivity.getSupportFragmentManager(),
                        "SystemMenuNotInstalledDialogFragment");
      }
    });
  }

  private void showAboutDialog()
  {
    new AboutDialogFragment().show(mActivity.getSupportFragmentManager(), AboutDialogFragment.TAG);
  }
}
