package org.dolphinemu.dolphinemu.ui.main;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.leanback.app.BrowseSupportFragment;
import androidx.leanback.widget.ArrayObjectAdapter;
import androidx.leanback.widget.HeaderItem;
import androidx.leanback.widget.ListRow;
import androidx.leanback.widget.ListRowPresenter;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.core.content.ContextCompat;

import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.adapters.GameRowPresenter;
import org.dolphinemu.dolphinemu.adapters.SettingsRowPresenter;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.model.TvSettingsItem;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;
import org.dolphinemu.dolphinemu.utils.TvUtil;
import org.dolphinemu.dolphinemu.viewholders.TvGameViewHolder;

import java.util.Collection;

public final class TvMainActivity extends FragmentActivity implements MainView
{
  private static boolean sShouldRescanLibrary = true;

  private MainPresenter mPresenter = new MainPresenter(this, this);

  private BrowseSupportFragment mBrowseFragment;

  private ArrayObjectAdapter mRowsAdapter;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_tv_main);

    setupUI();

    mPresenter.onCreate();

    // Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
    if (savedInstanceState == null)
    {
      StartupHandler.HandleInit(this);
    }
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    mPresenter.addDirIfNeeded(this);
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
    StartupHandler.setSessionTime(this);
  }

  void setupUI()
  {
    final FragmentManager fragmentManager = getSupportFragmentManager();
    mBrowseFragment = new BrowseSupportFragment();
    fragmentManager
            .beginTransaction()
            .add(R.id.content, mBrowseFragment, "BrowseFragment")
            .commit();

    // Set display parameters for the BrowseFragment
    mBrowseFragment.setHeadersState(BrowseSupportFragment.HEADERS_ENABLED);
    mBrowseFragment.setBrandColor(ContextCompat.getColor(this, R.color.dolphin_blue_dark));
    buildRowsAdapter();

    mBrowseFragment.setOnItemViewClickedListener(
            (itemViewHolder, item, rowViewHolder, row) ->
            {
              // Special case: user clicked on a settings row item.
              if (item instanceof TvSettingsItem)
              {
                TvSettingsItem settingsItem = (TvSettingsItem) item;
                mPresenter.handleOptionSelection(settingsItem.getItemId(), this);
              }
              else
              {
                TvGameViewHolder holder = (TvGameViewHolder) itemViewHolder;

                // Start the emulation activity and send the path of the clicked ISO to it.
                EmulationActivity.launch(TvMainActivity.this, holder.gameFile);
              }
            });
  }

  /**
   * MainView
   */

  @Override
  public void setVersionString(String version)
  {
    mBrowseFragment.setTitle(version);
  }

  @Override
  public void launchSettingsActivity(MenuTag menuTag)
  {
    SettingsActivity.launch(this, menuTag, "");
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
    FileBrowserHelper.openFilePicker(this, MainPresenter.REQUEST_WAD_FILE, false,
            FileBrowserHelper.WAD_EXTENSION);
  }

  @Override
  public void showGames()
  {
    // Kicks off the program services to update all channels
    TvUtil.updateAllChannels(getApplicationContext());

    buildRowsAdapter();
  }

  /**
   * Callback from AddDirectoryActivity. Applies any changes necessary to the GameGridActivity.
   *
   * @param requestCode An int describing whether the Activity that is returning did so successfully.
   * @param resultCode  An int describing what Activity is giving us this callback.
   * @param result      The information the returning Activity is providing us.
   */
  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent result)
  {
    super.onActivityResult(requestCode, resultCode, result);
    switch (requestCode)
    {
      case MainPresenter.REQUEST_DIRECTORY:
        // If the user picked a file, as opposed to just backing out.
        if (resultCode == MainActivity.RESULT_OK)
        {
          mPresenter.onDirectorySelected(FileBrowserHelper.getSelectedPath(result));
        }
        break;

      case MainPresenter.REQUEST_GAME_FILE:
        // If the user picked a file, as opposed to just backing out.
        if (resultCode == MainActivity.RESULT_OK)
        {
          EmulationActivity.launchFile(this, FileBrowserHelper.getSelectedFiles(result));
        }
        break;

      case MainPresenter.REQUEST_WAD_FILE:
        // If the user picked a file, as opposed to just backing out.
        if (resultCode == MainActivity.RESULT_OK)
        {
          mPresenter.installWAD(FileBrowserHelper.getSelectedPath(result));
        }
        break;
    }
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
          @NonNull int[] grantResults)
  {
    switch (requestCode)
    {
      case PermissionsHandler.REQUEST_CODE_WRITE_PERMISSION:
        if (grantResults[0] == PackageManager.PERMISSION_GRANTED)
        {
          DirectoryInitialization.start(this);
          GameFileCacheService.startLoad(this);
        }
        else
        {
          Toast.makeText(this, R.string.write_permission_needed, Toast.LENGTH_SHORT)
                  .show();
        }
        break;
      default:
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        break;
    }
  }

  private void buildRowsAdapter()
  {
    mRowsAdapter = new ArrayObjectAdapter(new ListRowPresenter());

    if (PermissionsHandler.hasWriteAccess(this))
    {
      GameFileCacheService.startLoad(this);
    }

    for (Platform platform : Platform.values())
    {
      ListRow row = buildGamesRow(platform, GameFileCacheService.getGameFilesForPlatform(platform));

      // Add row to the adapter only if it is not empty.
      if (row != null)
      {
        mRowsAdapter.add(row);
      }
    }

    mRowsAdapter.add(buildSettingsRow());

    mBrowseFragment.setAdapter(mRowsAdapter);
  }

  private ListRow buildGamesRow(Platform platform, Collection<GameFile> gameFiles)
  {
    // If there are no games, don't return a Row.
    if (gameFiles.size() == 0)
    {
      return null;
    }

    // Create an adapter for this row.
    ArrayObjectAdapter row = new ArrayObjectAdapter(new GameRowPresenter());
    row.addAll(0, gameFiles);

    // Create a header for this row.
    HeaderItem header = new HeaderItem(platform.toInt(), platform.getHeaderName());

    // Create the row, passing it the filled adapter and the header, and give it to the master adapter.
    return new ListRow(header, row);
  }

  private ListRow buildSettingsRow()
  {
    ArrayObjectAdapter rowItems = new ArrayObjectAdapter(new SettingsRowPresenter());

    rowItems.add(new TvSettingsItem(R.id.menu_settings_core,
            R.drawable.ic_settings_core_tv,
            R.string.grid_menu_config));

    rowItems.add(new TvSettingsItem(R.id.menu_settings_graphics,
            R.drawable.ic_settings_graphics_tv,
            R.string.grid_menu_graphics_settings));

    rowItems.add(new TvSettingsItem(R.id.menu_settings_gcpad,
            R.drawable.ic_settings_gcpad,
            R.string.grid_menu_gcpad_settings));

    rowItems.add(new TvSettingsItem(R.id.menu_settings_wiimote,
            R.drawable.ic_settings_wiimote,
            R.string.grid_menu_wiimote_settings));

    rowItems.add(new TvSettingsItem(R.id.button_add_directory,
            R.drawable.ic_add_tv,
            R.string.add_directory_title));

    rowItems.add(new TvSettingsItem(R.id.menu_refresh,
            R.drawable.ic_refresh_tv,
            R.string.grid_menu_refresh));

    rowItems.add(new TvSettingsItem(R.id.menu_open_file,
            R.drawable.ic_play,
            R.string.grid_menu_open_file));

    rowItems.add(new TvSettingsItem(R.id.menu_install_wad,
            R.drawable.ic_folder,
            R.string.grid_menu_install_wad));

    // Create a header for this row.
    HeaderItem header =
            new HeaderItem(R.string.preferences_settings, getString(R.string.preferences_settings));

    return new ListRow(header, rowItems);
  }

  public static void skipRescanningLibrary()
  {
    sShouldRescanLibrary = false;
  }
}
