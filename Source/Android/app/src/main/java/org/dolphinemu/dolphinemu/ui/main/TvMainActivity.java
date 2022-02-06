// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.util.TypedValue;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.leanback.app.BrowseSupportFragment;
import androidx.leanback.widget.ArrayObjectAdapter;
import androidx.leanback.widget.HeaderItem;
import androidx.leanback.widget.ListRow;
import androidx.leanback.widget.ListRowPresenter;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.adapters.GameRowPresenter;
import org.dolphinemu.dolphinemu.adapters.SettingsRowPresenter;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.model.TvSettingsItem;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;
import org.dolphinemu.dolphinemu.utils.TvUtil;
import org.dolphinemu.dolphinemu.viewholders.TvGameViewHolder;

import java.util.ArrayList;
import java.util.Collection;

public final class TvMainActivity extends FragmentActivity
        implements SwipeRefreshLayout.OnRefreshListener
{
  private final MainActivity mActivity = new MainActivity();

  private SwipeRefreshLayout mSwipeRefresh;

  private BrowseSupportFragment mBrowseFragment;

  private final ArrayList<ArrayObjectAdapter> mGameRows = new ArrayList<>();

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_tv_main);

    setupUI();

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

    if (DirectoryInitialization.shouldStart(this))
    {
      DirectoryInitialization.start(this);
      GameFileCacheManager.startLoad(this);
    }

    // In case the user changed a setting that affects how games are displayed,
    // such as system language, cover downloading...
    refetchMetadata();
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
      MainActivity.skipRescanningLibrary();
    }

    StartupHandler.setSessionTime(this);
  }

  void setupUI()
  {
    mSwipeRefresh = findViewById(R.id.swipe_refresh);

    TypedValue typedValue = new TypedValue();
    getTheme().resolveAttribute(R.attr.colorPrimary, typedValue, true);
    mSwipeRefresh.setColorSchemeColors(typedValue.data);

    mSwipeRefresh.setOnRefreshListener(this);

    setRefreshing(GameFileCacheManager.isLoadingOrRescanning());

    final FragmentManager fragmentManager = getSupportFragmentManager();
    mBrowseFragment = new BrowseSupportFragment();
    fragmentManager
            .beginTransaction()
            .add(R.id.content, mBrowseFragment, "BrowseFragment")
            .commit();

    // Set display parameters for the BrowseFragment
    mBrowseFragment.setHeadersState(BrowseSupportFragment.HEADERS_ENABLED);
    mBrowseFragment.setBrandColor(ContextCompat.getColor(this, R.color.dolphin_blue_secondary));
    buildRowsAdapter();

    mBrowseFragment.setOnItemViewClickedListener(
            (itemViewHolder, item, rowViewHolder, row) ->
            {
              // Special case: user clicked on a settings row item.
              if (item instanceof TvSettingsItem)
              {
                TvSettingsItem settingsItem = (TvSettingsItem) item;
                mActivity.handleOptionSelection(settingsItem.getItemId(), this);
              }
              else
              {
                TvGameViewHolder holder = (TvGameViewHolder) itemViewHolder;

                // Start the emulation activity and send the path of the clicked ISO to it.
                String[] paths = GameFileCacheManager.findSecondDiscAndGetPaths(holder.gameFile);
                EmulationActivity.launch(TvMainActivity.this, paths, false);
              }
            });
  }

  /**
   * MainView
   */

  public void setVersionString(String version)
  {
    mBrowseFragment.setTitle(version);
  }

  /**
   * Shows or hides the loading indicator.
   */
  public void setRefreshing(boolean refreshing)
  {
    mSwipeRefresh.setRefreshing(refreshing);
  }

  public void showGames()
  {
    // Kicks off the program services to update all channels
    TvUtil.updateAllChannels(getApplicationContext());

    buildRowsAdapter();
  }

  private void refetchMetadata()
  {
    for (ArrayObjectAdapter row : mGameRows)
    {
      row.notifyArrayItemRangeChanged(0, row.size());
    }
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

    // If the user picked a file, as opposed to just backing out.
    if (resultCode == RESULT_OK)
    {
      Uri uri = result.getData();
      switch (requestCode)
      {
        case MainActivity.REQUEST_DIRECTORY:
          if (DirectoryInitialization.preferOldFolderPicker(this))
          {
            mActivity.onDirectorySelected(FileBrowserHelper.getSelectedPath(result));
          }
          else
          {
            mActivity.onDirectorySelected(result);
          }
          break;

        case MainActivity.REQUEST_GAME_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri,
                  FileBrowserHelper.GAME_LIKE_EXTENSIONS,
                  () -> EmulationActivity.launch(this, result.getData().toString(), false));
          break;

        case MainActivity.REQUEST_WAD_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.WAD_EXTENSION,
                  () -> mActivity.installWAD(result.getData().toString()));
          break;

        case MainActivity.REQUEST_WII_SAVE_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.BIN_EXTENSION,
                  () -> mActivity.importWiiSave(result.getData().toString()));
          break;

        case MainActivity.REQUEST_NAND_BIN_FILE:
          FileBrowserHelper.runAfterExtensionCheck(this, uri, FileBrowserHelper.BIN_EXTENSION,
                  () -> mActivity.importNANDBin(result.getData().toString()));
          break;
      }
    }
    else
    {
      MainActivity.skipRescanningLibrary();
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
      GameFileCacheManager.startLoad(this);
    }
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

  private void buildRowsAdapter()
  {
    ArrayObjectAdapter rowsAdapter = new ArrayObjectAdapter(new ListRowPresenter());
    mGameRows.clear();

    if (!DirectoryInitialization.isWaitingForWriteAccess(this))
    {
      GameFileCacheManager.startLoad(this);
    }

    for (Platform platform : Platform.values())
    {
      ListRow row = buildGamesRow(platform, GameFileCacheManager.getGameFilesForPlatform(platform));

      // Add row to the adapter only if it is not empty.
      if (row != null)
      {
        rowsAdapter.add(row);
      }
    }

    rowsAdapter.add(buildSettingsRow());

    mBrowseFragment.setAdapter(rowsAdapter);
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

    // Keep a reference to the row in case we need to refresh it.
    mGameRows.add(row);

    // Create a header for this row.
    HeaderItem header = new HeaderItem(platform.toInt(), getString(platform.getHeaderName()));

    // Create the row, passing it the filled adapter and the header, and give it to the master adapter.
    return new ListRow(header, row);
  }

  private ListRow buildSettingsRow()
  {
    ArrayObjectAdapter rowItems = new ArrayObjectAdapter(new SettingsRowPresenter());

    rowItems.add(new TvSettingsItem(R.id.menu_settings,
            R.drawable.ic_settings_tv,
            R.string.grid_menu_settings));

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

    rowItems.add(new TvSettingsItem(R.id.menu_load_wii_system_menu,
            R.drawable.ic_folder,
            R.string.grid_menu_load_wii_system_menu));

    rowItems.add(new TvSettingsItem(R.id.menu_import_wii_save,
            R.drawable.ic_folder,
            R.string.grid_menu_import_wii_save));

    rowItems.add(new TvSettingsItem(R.id.menu_import_nand_backup,
            R.drawable.ic_folder,
            R.string.grid_menu_import_nand_backup));

    rowItems.add(new TvSettingsItem(R.id.menu_online_system_update,
            R.drawable.ic_folder,
            R.string.grid_menu_online_system_update));

    // Create a header for this row.
    HeaderItem header = new HeaderItem(R.string.settings, getString(R.string.settings));

    return new ListRow(header, rowItems);
  }
}
