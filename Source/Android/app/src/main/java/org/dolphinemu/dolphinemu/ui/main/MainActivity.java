// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main;

import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.activity.ComponentActivity;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.android.material.tabs.TabLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.adapters.PlatformPagerAdapter;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.features.sysupdate.ui.OnlineUpdateProgressBarDialogFragment;
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemMenuNotInstalledDialogFragment;
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemUpdateViewModel;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesView;
import org.dolphinemu.dolphinemu.utils.Action1;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.BooleanSupplier;
import org.dolphinemu.dolphinemu.utils.CompletableFuture;
import org.dolphinemu.dolphinemu.utils.ContentHandler;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;
import org.dolphinemu.dolphinemu.utils.ThreadUtil;
import org.dolphinemu.dolphinemu.utils.WiiUtils;
import org.dolphinemu.dolphinemu.BuildConfig;

import java.util.Arrays;
import java.util.concurrent.ExecutionException;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity
        implements SwipeRefreshLayout.OnRefreshListener
{
  public static final int REQUEST_DIRECTORY = 1;
  public static final int REQUEST_GAME_FILE = 2;
  public static final int REQUEST_SD_FILE = 3;
  public static final int REQUEST_WAD_FILE = 4;
  public static final int REQUEST_WII_SAVE_FILE = 5;
  public static final int REQUEST_NAND_BIN_FILE = 6;

  private ViewPager mViewPager;
  private Toolbar mToolbar;
  private TabLayout mTabLayout;
  private FloatingActionButton mFab;

  private String mDirToAdd;

  private static boolean sShouldRescanLibrary = true;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    findViews();

    setSupportActionBar(mToolbar);

    // Set up the FAB.
    mFab.setOnClickListener(view -> onFabClick());

    getPresenter();

    // Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
    if (savedInstanceState == null)
      StartupHandler.HandleInit(this);

    if (!DirectoryInitialization.isWaitingForWriteAccess(this))
    {
      new AfterDirectoryInitializationRunner()
              .runWithLifecycle(this, false, this::setPlatformTabsAndStartGameFileCacheService);
    }
  }

  public void getPresenter()
  {
    String versionName = BuildConfig.VERSION_NAME;
    setVersionString(versionName);

    GameFileCacheManager.getGameFiles().observe(this, (gameFiles) -> showGames());

    Observer<Boolean> refreshObserver = (isLoading) ->
    {
      setRefreshing(GameFileCacheManager.isLoadingOrRescanning());
    };
    GameFileCacheManager.isLoading().observe(this, refreshObserver);
    GameFileCacheManager.isRescanning().observe(this, refreshObserver);
  }

  @Override
  protected void onResume()
  {
    super.onResume();

    if (mDirToAdd != null)
    {
      GameFileCache.addGameFolder(mDirToAdd);
      mDirToAdd = null;
    }

    if (sShouldRescanLibrary)
    {
      GameFileCacheManager.startRescan(this);
    }

    sShouldRescanLibrary = true;

    if (DirectoryInitialization.shouldStart(this))
    {
      DirectoryInitialization.start(this);
      new AfterDirectoryInitializationRunner()
              .runWithLifecycle(this, false, this::setPlatformTabsAndStartGameFileCacheService);
    }

    // In case the user changed a setting that affects how games are displayed,
    // such as system language, cover downloading...
    forEachPlatformGamesView(PlatformGamesView::refetchMetadata);
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    StartupHandler.checkSessionReset(this);
  }

  @Override
  protected void onStop()
  {
    super.onStop();

    if (isChangingConfigurations())
    {
      skipRescanningLibrary();
    }
    else if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      // If the currently selected platform tab changed, save it to disk
      NativeConfig.save(NativeConfig.LAYER_BASE);
    }

    StartupHandler.setSessionTime(this);
  }

  // TODO: Replace with a ButterKnife injection.
  private void findViews()
  {
    mToolbar = findViewById(R.id.toolbar_main);
    mViewPager = findViewById(R.id.pager_platforms);
    mTabLayout = findViewById(R.id.tabs_platforms);
    mFab = findViewById(R.id.button_add_directory);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu)
  {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.menu_game_grid, menu);

    if (WiiUtils.isSystemMenuInstalled())
    {
      menu.findItem(R.id.menu_load_wii_system_menu).setTitle(
              getString(R.string.grid_menu_load_wii_system_menu_installed,
                      WiiUtils.getSystemMenuVersion()));
    }

    return true;
  }

  public boolean handleOptionSelection(int itemId, ComponentActivity activity)
  {
    switch (itemId)
    {
      case R.id.menu_settings:
        launchSettingsActivity(MenuTag.SETTINGS);
        return true;

      case R.id.menu_refresh:
        setRefreshing(true);
        GameFileCacheManager.startRescan(activity);
        return true;

      case R.id.button_add_directory:
        launchFileListActivity();
        return true;

      case R.id.menu_open_file:
        launchOpenFileActivity(REQUEST_GAME_FILE);
        return true;

      case R.id.menu_load_wii_system_menu:
        launchWiiSystemMenu();
        return true;

      case R.id.menu_online_system_update:
        launchOnlineUpdate();
        return true;

      case R.id.menu_install_wad:
        new AfterDirectoryInitializationRunner().runWithLifecycle(activity, true,
                () -> launchOpenFileActivity(REQUEST_WAD_FILE));
        return true;

      case R.id.menu_import_wii_save:
        new AfterDirectoryInitializationRunner().runWithLifecycle(activity, true,
                () -> launchOpenFileActivity(REQUEST_WII_SAVE_FILE));
        return true;

      case R.id.menu_import_nand_backup:
        new AfterDirectoryInitializationRunner().runWithLifecycle(activity, true,
                () -> launchOpenFileActivity(REQUEST_NAND_BIN_FILE));
        return true;
    }

    return false;
  }

  public void setVersionString(String version)
  {
    mToolbar.setSubtitle(version);
  }

  public void launchSettingsActivity(MenuTag menuTag)
  {
    SettingsActivity.launch(this, menuTag);
  }

  public void launchFileListActivity()
  {
    if (DirectoryInitialization.preferOldFolderPicker(this))
    {
      FileBrowserHelper.openDirectoryPicker(this, FileBrowserHelper.GAME_EXTENSIONS);
    }
    else
    {
      Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
      startActivityForResult(intent, REQUEST_DIRECTORY);
    }
  }

  public void launchOpenFileActivity(int requestCode)
  {
    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
    intent.addCategory(Intent.CATEGORY_OPENABLE);
    intent.setType("*/*");
    startActivityForResult(intent, requestCode);
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
      AlertDialog.Builder builder = new AlertDialog.Builder(this, R.style.DolphinDialogBase);
      builder.setMessage(getString(R.string.wrong_file_extension_in_directory,
              FileBrowserHelper.setToSortedDelimitedString(FileBrowserHelper.GAME_EXTENSIONS)));
      builder.setPositiveButton(R.string.ok, null);
      builder.show();
    }

    ContentResolver contentResolver = getContentResolver();
    Uri canonicalizedUri = contentResolver.canonicalize(uri);
    if (canonicalizedUri != null)
      uri = canonicalizedUri;

    int takeFlags = result.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
    getContentResolver().takePersistableUriPermission(uri, takeFlags);

    mDirToAdd = uri.toString();
  }

  /**
   * Called when a selection is made using the legacy folder picker.
   */
  public void onDirectorySelected(String dir)
  {
    mDirToAdd = dir;
  }

  public void onFabClick()
  {
    launchFileListActivity();
  }

  public void installWAD(String path)
  {
    ThreadUtil.runOnThreadAndShowResult(this, R.string.import_in_progress, 0, () ->
    {
      boolean success = WiiUtils.installWAD(path);
      int message = success ? R.string.wad_install_success : R.string.wad_install_failure;
      return getResources().getString(message);
    });
  }

  public void importWiiSave(String path)
  {
    CompletableFuture<Boolean> canOverwriteFuture = new CompletableFuture<>();

    ThreadUtil.runOnThreadAndShowResult(this, R.string.import_in_progress, 0, () ->
    {
      BooleanSupplier canOverwrite = () ->
      {
        runOnUiThread(() ->
        {
          AlertDialog.Builder builder =
                  new AlertDialog.Builder(this, R.style.DolphinDialogBase);
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
      return getResources().getString(message);
    });
  }

  public void importNANDBin(String path)
  {
    AlertDialog.Builder builder =
            new AlertDialog.Builder(this, R.style.DolphinDialogBase);

    builder.setMessage(R.string.nand_import_warning);
    builder.setNegativeButton(R.string.no, (dialog, i) -> dialog.dismiss());
    builder.setPositiveButton(R.string.yes, (dialog, i) ->
    {
      dialog.dismiss();

      ThreadUtil.runOnThreadAndShowResult(this, R.string.import_in_progress,
              R.string.do_not_close_app, () ->
              {
                // ImportNANDBin unfortunately doesn't provide any result value...
                // It does however show a panic alert if something goes wrong.
                WiiUtils.importNANDBin(path);
                return null;
              });
    });

    builder.show();
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
              new ViewModelProvider(this).get(SystemUpdateViewModel.class);
      viewModel.setRegion(-1);
      OnlineUpdateProgressBarDialogFragment progressBarFragment =
              new OnlineUpdateProgressBarDialogFragment();
      progressBarFragment
              .show(getSupportFragmentManager(), "OnlineUpdateProgressBarDialogFragment");
      progressBarFragment.setCancelable(false);
    }
    else
    {
      SystemMenuNotInstalledDialogFragment dialogFragment =
              new SystemMenuNotInstalledDialogFragment();
      dialogFragment
              .show(getSupportFragmentManager(), "SystemMenuNotInstalledDialogFragment");
    }
  }

  private void launchWiiSystemMenu()
  {
    WiiUtils.isSystemMenuInstalled();

    if (WiiUtils.isSystemMenuInstalled())
    {
      EmulationActivity.launchSystemMenu(this);
    }
    else
    {
      SystemMenuNotInstalledDialogFragment dialogFragment =
              new SystemMenuNotInstalledDialogFragment();
      dialogFragment
              .show(getSupportFragmentManager(), "SystemMenuNotInstalledDialogFragment");
    }
  }

  /**
   * @param requestCode An int describing whether the Activity that is returning did so successfully.
   * @param resultCode  An int describing what Activity is giving us this callback.
   * @param result      The information the returning Activity is providing us.
   */
  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent result)
  {
    super.onActivityResult(requestCode, resultCode, result);

    // If the user picked a file, as opposed to just backing out.
    if (resultCode == RESULT_OK)
    {
      Uri uri = result.getData();
      switch (requestCode)
      {
        case REQUEST_DIRECTORY:
          if (DirectoryInitialization.preferOldFolderPicker(this))
          {
            onDirectorySelected(FileBrowserHelper.getSelectedPath(result));
          }
          else
          {
            onDirectorySelected(result);
          }
          break;

        case REQUEST_GAME_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri,
                  FileBrowserHelper.GAME_LIKE_EXTENSIONS,
                  () -> EmulationActivity.launch(this, result.getData().toString(), false));
          break;

        case REQUEST_WAD_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.WAD_EXTENSION,
                  () -> installWAD(result.getData().toString()));
          break;

        case REQUEST_WII_SAVE_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.BIN_EXTENSION,
                  () -> importWiiSave(result.getData().toString()));
          break;

        case REQUEST_NAND_BIN_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.BIN_EXTENSION,
                  () -> importNANDBin(result.getData().toString()));
          break;
      }
    }
    else
    {
      skipRescanningLibrary();
    }
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
          @NonNull int[] grantResults)
  {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);

    if (requestCode == PermissionsHandler.REQUEST_CODE_WRITE_PERMISSION)
    {
      if (grantResults[0] == PackageManager.PERMISSION_DENIED)
      {
        PermissionsHandler.setWritePermissionDenied();
      }

      DirectoryInitialization.start(this);
      new AfterDirectoryInitializationRunner()
              .runWithLifecycle(this, false, this::setPlatformTabsAndStartGameFileCacheService);
    }
  }

  /**
   * Called by the framework whenever any actionbar/toolbar icon is clicked.
   *
   * @param item The icon that was clicked on.
   * @return True if the event was handled, false to bubble it up to the OS.
   */
  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    return handleOptionSelection(item.getItemId(), this);
  }

  /**
   * Called when the user requests a refresh by swiping down.
   */
  @Override
  public void onRefresh()
  {
    setRefreshing(true);
    GameFileCacheManager.startRescan(this);
  }

  /**
   * Shows or hides the loading indicator.
   */
  public void setRefreshing(boolean refreshing)
  {
    forEachPlatformGamesView(view -> view.setRefreshing(refreshing));
  }

  /**
   * To be called when the game file cache is updated.
   */
  public void showGames()
  {
    forEachPlatformGamesView(PlatformGamesView::showGames);
  }

  private void forEachPlatformGamesView(Action1<PlatformGamesView> action)
  {
    for (Platform platform : Platform.values())
    {
      PlatformGamesView fragment = getPlatformGamesView(platform);
      if (fragment != null)
      {
        action.call(fragment);
      }
    }
  }

  @Nullable
  private PlatformGamesView getPlatformGamesView(Platform platform)
  {
    String fragmentTag = "android:switcher:" + mViewPager.getId() + ":" + platform.toInt();

    return (PlatformGamesView) getSupportFragmentManager().findFragmentByTag(fragmentTag);
  }

  // Don't call this before DirectoryInitialization completes.
  private void setPlatformTabsAndStartGameFileCacheService()
  {
    PlatformPagerAdapter platformPagerAdapter = new PlatformPagerAdapter(
            getSupportFragmentManager(), this, this);
    mViewPager.setAdapter(platformPagerAdapter);
    mViewPager.setOffscreenPageLimit(platformPagerAdapter.getCount());
    mTabLayout.setupWithViewPager(mViewPager);
    mTabLayout.addOnTabSelectedListener(new TabLayout.ViewPagerOnTabSelectedListener(mViewPager)
    {
      @Override
      public void onTabSelected(@NonNull TabLayout.Tab tab)
      {
        super.onTabSelected(tab);
        IntSetting.MAIN_LAST_PLATFORM_TAB.setIntGlobal(NativeConfig.LAYER_BASE, tab.getPosition());
      }
    });

    mViewPager.setCurrentItem(IntSetting.MAIN_LAST_PLATFORM_TAB.getIntGlobal());

    showGames();
    GameFileCacheManager.startLoad(this);
  }
}