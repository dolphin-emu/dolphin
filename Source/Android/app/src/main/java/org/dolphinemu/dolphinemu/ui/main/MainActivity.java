// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.splashscreen.SplashScreen;
import androidx.core.view.WindowCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import com.google.android.material.bottomnavigation.BottomNavigationView;
import com.google.android.material.navigation.NavigationBarView;

import org.dolphinemu.dolphinemu.MoreFragment;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.databinding.ActivityMainBinding;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesFragment;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesView;
import org.dolphinemu.dolphinemu.utils.Action1;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;
import org.dolphinemu.dolphinemu.utils.TvUtil;

public final class MainActivity extends AppCompatActivity implements MainView, ThemeProvider
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
    InsetsHelper.setUpMainLayout(this, mBinding.appbarMain, mBinding.buttonAddDirectory,
            mBinding.frameMain, mBinding.navigation, mBinding.workaroundView);
    ThemeHelper.enableStatusBarScrollTint(this, mBinding.appbarMain);

    setSupportActionBar(mBinding.toolbarMain);

    // Set up the FAB.
    mBinding.buttonAddDirectory.setOnClickListener(view -> mPresenter.onFabClick());

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

    // Set up Navigation View
    setUpNavigation();
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
    if (TvUtil.isLeanback(getApplicationContext()))
    {
      TvUtil.updateAllChannels(getApplicationContext());
    }

    forEachPlatformGamesView(PlatformGamesView::showGames);
  }

  private void forEachPlatformGamesView(Action1<PlatformGamesView> action)
  {
    for (Tab tab : Tab.values())
    {
      PlatformGamesView fragment = getPlatformGamesView(tab);
      if (fragment != null)
      {
        action.call(fragment);
      }
    }
  }

  @Nullable
  private PlatformGamesView getPlatformGamesView(Tab tab)
  {
    Fragment fragment = getSupportFragmentManager().findFragmentByTag(tab.getIdString());
    if (fragment instanceof PlatformGamesView)
    {
      return (PlatformGamesView) fragment;
    }
    return null;
  }

  private void setUpNavigation()
  {
    ((NavigationBarView) mBinding.navigation).setOnItemSelectedListener(
            item ->
            {
              switch (item.getItemId())
              {
                case R.id.action_gamecube:
                  changeFragment(PlatformGamesFragment.newInstance(Platform.GAMECUBE),
                          Tab.GAMECUBE.getIdString());
                  IntSetting.MAIN_LAST_PLATFORM_TAB.setIntGlobal(NativeConfig.LAYER_BASE,
                          Tab.GAMECUBE.toInt());
                  return true;
                case R.id.action_wii:
                  changeFragment(PlatformGamesFragment.newInstance(Platform.WII),
                          Tab.WII.getIdString());
                  IntSetting.MAIN_LAST_PLATFORM_TAB.setIntGlobal(NativeConfig.LAYER_BASE,
                          Tab.WII.toInt());
                  return true;
                case R.id.action_wiiware:
                  changeFragment(PlatformGamesFragment.newInstance(Platform.WIIWARE),
                          Tab.WIIWARE.getIdString());
                  IntSetting.MAIN_LAST_PLATFORM_TAB.setIntGlobal(NativeConfig.LAYER_BASE,
                          Tab.WIIWARE.toInt());
                  return true;
                case R.id.action_more:
                  // TODO: MAKE THIS FRAGMENT
                  mBinding.appbarMain.setExpanded(true);
                  changeFragment(new MoreFragment(), Tab.MORE.getIdString());
                  IntSetting.MAIN_LAST_PLATFORM_TAB.setIntGlobal(NativeConfig.LAYER_BASE,
                          Tab.MORE.toInt());
                  return true;
              }
              return false;
            });

    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      ((NavigationBarView) mBinding.navigation).setSelectedItemId(
              Tab.getTabId(IntSetting.MAIN_LAST_PLATFORM_TAB.getIntGlobal()));
    }

    // Hide FAB when scrolling down
    if (mBinding.navigation instanceof BottomNavigationView)
    {
      mBinding.appbarMain.addOnOffsetChangedListener((appBarLayout, verticalOffset) ->
      {
        if (-verticalOffset == appBarLayout.getTotalScrollRange())
        {
          mBinding.buttonAddDirectory.hide();
        }
        else if (!mBinding.buttonAddDirectory.isShown() ||
                -verticalOffset < appBarLayout.getTotalScrollRange())
        {
          mBinding.buttonAddDirectory.show();
        }
      });
    }
  }

  // Don't call this before DirectoryInitialization completes.
  private void setPlatformTabsAndStartGameFileCacheService()
  {
    showGames();
    GameFileCacheManager.startLoad();

    ((NavigationBarView) mBinding.navigation).setSelectedItemId(
            Tab.getTabId(IntSetting.MAIN_LAST_PLATFORM_TAB.getIntGlobal()));
  }

  // Solution from https://stackoverflow.com/a/53203785
  // This is the best way to ensure fragments are reused
  private void changeFragment(Fragment fragment, String fragmentTag)
  {
    FragmentManager manager = getSupportFragmentManager();
    FragmentTransaction transaction = manager.beginTransaction();
    Fragment currentFragment = manager.getPrimaryNavigationFragment();

    transaction.setCustomAnimations(R.anim.anim_fragment_in, R.anim.anim_fragment_out);
    if (currentFragment != null)
    {
      transaction.hide(currentFragment);
    }

    Fragment targetFragment = manager.findFragmentByTag(fragmentTag);
    if (targetFragment == null)
    {
      targetFragment = fragment;
      transaction.add(R.id.frame_main, targetFragment, fragmentTag);
    }
    else
    {
      transaction.show(targetFragment);
    }

    transaction
            .setPrimaryNavigationFragment(targetFragment)
            .setReorderingAllowed(true)
            .commitAllowingStateLoss();
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
}
