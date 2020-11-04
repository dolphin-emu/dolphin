package org.dolphinemu.dolphinemu.ui.main;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.android.material.tabs.TabLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.adapters.PlatformPagerAdapter;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesView;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity implements MainView
{
  private ViewPager mViewPager;
  private Toolbar mToolbar;
  private TabLayout mTabLayout;
  private FloatingActionButton mFab;
  private static boolean sShouldRescanLibrary = true;

  private MainPresenter mPresenter = new MainPresenter(this, this);

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

    if (PermissionsHandler.hasWriteAccess(this))
    {
      new AfterDirectoryInitializationRunner()
              .run(this, false, this::setPlatformTabsAndStartGameFileCacheService);
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
              .run(this, false, this::setPlatformTabsAndStartGameFileCacheService);
    }

    mPresenter.addDirIfNeeded(this);

    // In case the user changed a setting that affects how games are displayed,
    // such as system language, cover downloading...
    refetchMetadata();

    if (sShouldRescanLibrary)
    {
      GameFileCacheService.startRescan(this);
    }
    else
    {
      sShouldRescanLibrary = true;
    }
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
      skipRescanningLibrary();
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
    FileBrowserHelper.openDirectoryPicker(this, FileBrowserHelper.GAME_EXTENSIONS);
  }

  @Override
  public void launchOpenFileActivity()
  {
    FileBrowserHelper.openFilePicker(this, MainPresenter.REQUEST_GAME_FILE, false,
            FileBrowserHelper.GAME_EXTENSIONS);
  }

  @Override
  public void launchInstallWAD()
  {
    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
    intent.addCategory(Intent.CATEGORY_OPENABLE);
    intent.setType("*/*");
    startActivityForResult(intent, MainPresenter.REQUEST_WAD_FILE);
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
    if (resultCode == MainActivity.RESULT_OK)
    {
      switch (requestCode)
      {
        case MainPresenter.REQUEST_DIRECTORY:
          mPresenter.onDirectorySelected(FileBrowserHelper.getSelectedPath(result));
          break;

        case MainPresenter.REQUEST_GAME_FILE:
          EmulationActivity.launch(this, FileBrowserHelper.getSelectedFiles(result));
          break;

        case MainPresenter.REQUEST_WAD_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, result.getData(),
                  FileBrowserHelper.WAD_EXTENSION,
                  () -> mPresenter.installWAD(result.getData().toString()));
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
      if (grantResults[0] == PackageManager.PERMISSION_GRANTED)
      {
        DirectoryInitialization.start(this);
        new AfterDirectoryInitializationRunner()
                .run(this, false, this::setPlatformTabsAndStartGameFileCacheService);
      }
      else
      {
        Toast.makeText(this, R.string.write_permission_needed, Toast.LENGTH_LONG).show();
      }
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

  public void showGames()
  {
    for (Platform platform : Platform.values())
    {
      PlatformGamesView fragment = getPlatformGamesView(platform);
      if (fragment != null)
      {
        fragment.showGames();
      }
    }
  }

  private void refetchMetadata()
  {
    for (Platform platform : Platform.values())
    {
      PlatformGamesView fragment = getPlatformGamesView(platform);
      if (fragment != null)
      {
        fragment.refetchMetadata();
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
            getSupportFragmentManager(), this);
    mViewPager.setAdapter(platformPagerAdapter);
    mViewPager.setOffscreenPageLimit(platformPagerAdapter.getCount());
    mTabLayout.setupWithViewPager(mViewPager);
    mTabLayout.addOnTabSelectedListener(new TabLayout.ViewPagerOnTabSelectedListener(mViewPager)
    {
      @Override
      public void onTabSelected(@NonNull TabLayout.Tab tab)
      {
        super.onTabSelected(tab);

        try (Settings settings = new Settings())
        {
          settings.loadSettings(null);

          IntSetting.MAIN_LAST_PLATFORM_TAB.setInt(settings, tab.getPosition());

          // Context is set to null to avoid toasts
          settings.saveSettings(null, null);
        }
      }
    });

    mViewPager.setCurrentItem(IntSetting.MAIN_LAST_PLATFORM_TAB.getIntGlobal());

    showGames();
    GameFileCacheService.startLoad(this);
  }

  public static void skipRescanningLibrary()
  {
    sShouldRescanLibrary = false;
  }
}
