// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.splashscreen.SplashScreen;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.tabs.TabLayout;

import org.dolphinemu.dolphinemu.fragments.GridOptionDialogFragment;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.adapters.PlatformPagerAdapter;
import org.dolphinemu.dolphinemu.databinding.ActivityMainBinding;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesView;
import org.dolphinemu.dolphinemu.utils.Action1;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;
import org.dolphinemu.dolphinemu.utils.WiiUtils;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity
        implements MainView, SwipeRefreshLayout.OnRefreshListener, ThemeProvider
{
  private int mThemeId;

  private final MainPresenter mPresenter = new MainPresenter(this, this);

  private ActivityMainBinding mBinding;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    SplashScreen splashScreen = SplashScreen.installSplashScreen(this);
    splashScreen.setKeepOnScreenCondition(
            () -> !DirectoryInitialization.areDolphinDirectoriesReady());

    ThemeHelper.setTheme(this);

    super.onCreate(savedInstanceState);

    mBinding = ActivityMainBinding.inflate(getLayoutInflater());
    setContentView(mBinding.getRoot());

    WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
    setInsets();
    ThemeHelper.enableStatusBarScrollTint(this, mBinding.appbarMain);

    mBinding.toolbarMain.setTitle(R.string.app_name);
    setSupportActionBar(mBinding.toolbarMain);

    // Set up the FAB.
    mBinding.buttonAddDirectory.setOnClickListener(view -> mPresenter.onFabClick());
    mBinding.appbarMain.addOnOffsetChangedListener((appBarLayout, verticalOffset) ->
    {
      if (verticalOffset == 0)
      {
        mBinding.buttonAddDirectory.extend();
      }
      else if (appBarLayout.getTotalScrollRange() == -verticalOffset)
      {
        mBinding.buttonAddDirectory.shrink();
      }
    });

    mPresenter.onCreate();

    // Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
    if (savedInstanceState == null)
    {
      StartupHandler.HandleInit(this);
      new AfterDirectoryInitializationRunner().runWithLifecycle(this, this::checkTheme);
    }

    if (!DirectoryInitialization.isWaitingForWriteAccess(this))
    {
      new AfterDirectoryInitializationRunner()
              .runWithLifecycle(this, this::setPlatformTabsAndStartGameFileCacheService);
    }
  }

  @Override
  protected void onResume()
  {
    ThemeHelper.setCorrectTheme(this);

    super.onResume();

    if (DirectoryInitialization.shouldStart(this))
    {
      DirectoryInitialization.start(this);
      new AfterDirectoryInitializationRunner()
              .runWithLifecycle(this, this::setPlatformTabsAndStartGameFileCacheService);
    }

    mPresenter.onResume();
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
    mPresenter.onDestroy();
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
      MainPresenter.skipRescanningLibrary();
    }
    else if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      // If the currently selected platform tab changed, save it to disk
      NativeConfig.save(NativeConfig.LAYER_BASE);
    }

    StartupHandler.setSessionTime(this);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu)
  {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.menu_game_grid, menu);

    if (WiiUtils.isSystemMenuInstalled())
    {
      int resId = WiiUtils.isSystemMenuvWii() ?
              R.string.grid_menu_load_vwii_system_menu_installed :
              R.string.grid_menu_load_wii_system_menu_installed;

      menu.findItem(R.id.menu_load_wii_system_menu).setTitle(
              getString(resId, WiiUtils.getSystemMenuVersion()));
    }

    return true;
  }

  /**
   * MainView
   */

  @Override
  public void setVersionString(String version)
  {
    mBinding.toolbarMain.setSubtitle(version);
  }

  @Override
  public void launchSettingsActivity(MenuTag menuTag)
  {
    SettingsActivity.launch(this, menuTag);
  }

  @Override
  public void launchFileListActivity()
  {
    if (DirectoryInitialization.preferOldFolderPicker(this))
    {
      FileBrowserHelper.openDirectoryPicker(this, FileBrowserHelper.GAME_EXTENSIONS);
    }
    else
    {
      Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
      startActivityForResult(intent, MainPresenter.REQUEST_DIRECTORY);
    }
  }

  @Override
  public void launchOpenFileActivity(int requestCode)
  {
    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
    intent.addCategory(Intent.CATEGORY_OPENABLE);
    intent.setType("*/*");
    startActivityForResult(intent, requestCode);
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
        case MainPresenter.REQUEST_DIRECTORY:
          if (DirectoryInitialization.preferOldFolderPicker(this))
          {
            mPresenter.onDirectorySelected(FileBrowserHelper.getSelectedPath(result));
          }
          else
          {
            mPresenter.onDirectorySelected(result);
          }
          break;

        case MainPresenter.REQUEST_GAME_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri,
                  FileBrowserHelper.GAME_LIKE_EXTENSIONS,
                  () -> EmulationActivity.launch(this, result.getData().toString(), false));
          break;

        case MainPresenter.REQUEST_WAD_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.WAD_EXTENSION,
                  () -> mPresenter.installWAD(result.getData().toString()));
          break;

        case MainPresenter.REQUEST_WII_SAVE_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.BIN_EXTENSION,
                  () -> mPresenter.importWiiSave(result.getData().toString()));
          break;

        case MainPresenter.REQUEST_NAND_BIN_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.BIN_EXTENSION,
                  () -> mPresenter.importNANDBin(result.getData().toString()));
          break;
      }
    }
    else
    {
      MainPresenter.skipRescanningLibrary();
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
              .runWithLifecycle(this, this::setPlatformTabsAndStartGameFileCacheService);
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
    return mPresenter.handleOptionSelection(item.getItemId(), this);
  }

  /**
   * Called when the user requests a refresh by swiping down.
   */
  @Override
  public void onRefresh()
  {
    setRefreshing(true);
    GameFileCacheManager.startRescan();
  }

  /**
   * Shows or hides the loading indicator.
   */
  @Override
  public void setRefreshing(boolean refreshing)
  {
    forEachPlatformGamesView(view -> view.setRefreshing(refreshing));
  }

  /**
   * To be called when the game file cache is updated.
   */
  @Override
  public void showGames()
  {
    forEachPlatformGamesView(PlatformGamesView::showGames);
  }

  @Override
  public void reloadGrid()
  {
    forEachPlatformGamesView(PlatformGamesView::refetchMetadata);
  }

  @Override
  public void showGridOptions()
  {
    new GridOptionDialogFragment().show(getSupportFragmentManager(), "gridOptions");
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
    String fragmentTag =
            "android:switcher:" + mBinding.pagerPlatforms.getId() + ":" + platform.toInt();

    return (PlatformGamesView) getSupportFragmentManager().findFragmentByTag(fragmentTag);
  }

  // Don't call this before DirectoryInitialization completes.
  private void setPlatformTabsAndStartGameFileCacheService()
  {
    PlatformPagerAdapter platformPagerAdapter = new PlatformPagerAdapter(
            getSupportFragmentManager(), this);
    mBinding.pagerPlatforms.setAdapter(platformPagerAdapter);
    mBinding.pagerPlatforms.setOffscreenPageLimit(platformPagerAdapter.getCount());
    mBinding.tabsPlatforms.setupWithViewPager(mBinding.pagerPlatforms);
    mBinding.tabsPlatforms.addOnTabSelectedListener(
            new TabLayout.ViewPagerOnTabSelectedListener(mBinding.pagerPlatforms)
            {
              @Override
              public void onTabSelected(@NonNull TabLayout.Tab tab)
              {
                super.onTabSelected(tab);
                IntSetting.MAIN_LAST_PLATFORM_TAB.setInt(NativeConfig.LAYER_BASE,
                        tab.getPosition());
              }
            });

    for (int i = 0; i < PlatformPagerAdapter.TAB_ICONS.length; i++)
    {
      mBinding.tabsPlatforms.getTabAt(i).setIcon(PlatformPagerAdapter.TAB_ICONS[i]);
    }

    mBinding.pagerPlatforms.setCurrentItem(IntSetting.MAIN_LAST_PLATFORM_TAB.getInt());

    showGames();
    GameFileCacheManager.startLoad();
  }

  @Override
  public void setTheme(int themeId)
  {
    super.setTheme(themeId);
    this.mThemeId = themeId;
  }

  @Override
  public int getThemeId()
  {
    return mThemeId;
  }

  private void checkTheme()
  {
    ThemeHelper.setCorrectTheme(this);
  }

  private void setInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mBinding.appbarMain, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());

      InsetsHelper.insetAppBar(insets, mBinding.appbarMain);

      ViewGroup.MarginLayoutParams mlpFab =
              (ViewGroup.MarginLayoutParams) mBinding.buttonAddDirectory.getLayoutParams();
      int fabPadding = getResources().getDimensionPixelSize(R.dimen.spacing_large);
      mlpFab.leftMargin = insets.left + fabPadding;
      mlpFab.bottomMargin = insets.bottom + fabPadding;
      mlpFab.rightMargin = insets.right + fabPadding;
      mBinding.buttonAddDirectory.setLayoutParams(mlpFab);

      mBinding.pagerPlatforms.setPadding(insets.left, 0, insets.right, 0);

      InsetsHelper.applyNavbarWorkaround(insets.bottom, mBinding.workaroundView);
      ThemeHelper.setNavigationBarColor(this,
              MaterialColors.getColor(mBinding.appbarMain, R.attr.colorSurface));

      return windowInsets;
    });
  }
}
