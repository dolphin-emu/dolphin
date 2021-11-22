// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.android.material.tabs.TabLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.adapters.PlatformPagerAdapter;
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
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity
        implements MainView, SwipeRefreshLayout.OnRefreshListener
{
  private ViewPager mViewPager;
  private Toolbar mToolbar;
  private TabLayout mTabLayout;
  private FloatingActionButton mFab;

  private final MainPresenter mPresenter = new MainPresenter(this, this);

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    findViews();

    setSupportActionBar(mToolbar);

    // Set up the FAB.
    mFab.setOnClickListener(view -> mPresenter.onFabClick());

    mPresenter.onCreate();

    // Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
    if (savedInstanceState == null)
      StartupHandler.HandleInit(this);

    if (!DirectoryInitialization.isWaitingForWriteAccess(this))
    {
      new AfterDirectoryInitializationRunner()
              .runWithLifecycle(this, false, this::setPlatformTabsAndStartGameFileCacheService);
    }
  }

  @Override
  protected void onResume()
  {
    super.onResume();

    if (DirectoryInitialization.shouldStart(this))
    {
      DirectoryInitialization.start(this);
      new AfterDirectoryInitializationRunner()
              .runWithLifecycle(this, false, this::setPlatformTabsAndStartGameFileCacheService);
    }

    mPresenter.onResume();

    // In case the user changed a setting that affects how games are displayed,
    // such as system language, cover downloading...
    forEachPlatformGamesView(PlatformGamesView::refetchMetadata);
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
    return true;
  }

  /**
   * MainView
   */

  @Override
  public void setVersionString(String version)
  {
    mToolbar.setSubtitle(version);
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
    return mPresenter.handleOptionSelection(item.getItemId(), this);
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
