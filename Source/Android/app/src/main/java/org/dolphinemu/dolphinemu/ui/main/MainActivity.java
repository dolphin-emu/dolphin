package org.dolphinemu.dolphinemu.ui.main;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;

import com.nononsenseapps.filepicker.DividerItemDecoration;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.model.GameFileCache;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;

import java.io.File;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity
{
  public static final int REQUEST_ADD_DIRECTORY = 1;
  public static final int REQUEST_OPEN_FILE = 2;
  private static final String PREF_GAMELIST = "GAME_LIST_TYPE";
  private static final byte[] TITLE_BYTES = {
    0x44, 0x6f, 0x6c, 0x70, 0x68, 0x69, 0x6e, 0x20, 0x35, 0x2e, 0x30, 0x28, 0x4d, 0x4d, 0x4a, 0x29};
  private DividerItemDecoration mDivider;
  private RecyclerView mGameList;
  private GameAdapter mAdapter;
  private Toolbar mToolbar;
  private BroadcastReceiver mBroadcastReceiver;
  private String mDirToAdd;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.activity_main);
    findViews();
    setSupportActionBar(mToolbar);
    setTitle(new String(TITLE_BYTES));

    IntentFilter filter = new IntentFilter();
    filter.addAction(GameFileCacheService.BROADCAST_ACTION);
    mBroadcastReceiver = new BroadcastReceiver()
    {
      @Override
      public void onReceive(Context context, Intent intent)
      {
        showGames();
      }
    };
    LocalBroadcastManager.getInstance(this).registerReceiver(mBroadcastReceiver, filter);

    // Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
    if (savedInstanceState == null)
      StartupHandler.HandleInit(this);

    if (PermissionsHandler.hasWriteAccess(this))
    {
      showGames();
      GameFileCacheService.startLoad(this);
    }
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    if (mDirToAdd != null)
    {
      GameFileCache.addGameFolder(mDirToAdd, this);
      mDirToAdd = null;
      GameFileCacheService.startRescan(this);
    }
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
    if (mBroadcastReceiver != null)
    {
      LocalBroadcastManager.getInstance(this).unregisterReceiver(mBroadcastReceiver);
    }
  }

  // TODO: Replace with a ButterKnife injection.
  private void findViews()
  {
    SharedPreferences pref = PreferenceManager.getDefaultSharedPreferences(this);
    Drawable lineDivider = getDrawable(R.drawable.line_divider);
    mDivider = new DividerItemDecoration(lineDivider);
    mToolbar = findViewById(R.id.toolbar_main);
    mGameList = findViewById(R.id.grid_games);
    mAdapter = new GameAdapter();
    mGameList.setAdapter(mAdapter);
    refreshGameList(pref.getBoolean(PREF_GAMELIST, true));
  }

  private void refreshGameList(boolean flag)
  {
    int resourceId;
    int columns = getResources().getInteger(R.integer.game_grid_columns);
    RecyclerView.LayoutManager layoutManager;
    if (flag)
    {
      resourceId = R.layout.card_game;
      layoutManager = new GridLayoutManager(this, columns);
      mGameList.addItemDecoration(mDivider);
    }
    else
    {
      columns = columns * 2 + 1;
      resourceId = R.layout.card_game2;
      layoutManager = new GridLayoutManager(this, columns);
      mGameList.removeItemDecoration(mDivider);
    }
    mAdapter.setResourceId(resourceId);
    mGameList.setLayoutManager(layoutManager);
  }

  public void toggleGameList()
  {
    SharedPreferences pref = PreferenceManager.getDefaultSharedPreferences(this);
    boolean flag = !pref.getBoolean(PREF_GAMELIST, true);
    pref.edit().putBoolean(PREF_GAMELIST, flag).apply();
    refreshGameList(flag);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu)
  {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.menu_game_grid, menu);
    return true;
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    switch (item.getItemId())
    {
      case R.id.menu_add_directory:
        launchFileListActivity();
        return true;

      case R.id.menu_toggle_list:
        toggleGameList();
        return true;

      case R.id.menu_settings_core:
        launchSettingsActivity(MenuTag.CONFIG);
        return true;

      case R.id.menu_settings_gcpad:
        launchSettingsActivity(MenuTag.GCPAD_TYPE);
        return true;

      case R.id.menu_settings_wiimote:
        launchSettingsActivity(MenuTag.WIIMOTE);
        return true;

      case R.id.menu_clear_data:
        clearGameData(this);
        return true;

      case R.id.menu_refresh:
        GameFileCacheService.startRescan(this);
        return true;

      case R.id.menu_open_file:
        launchOpenFileActivity();
        return true;
    }

    return false;
  }

  public void launchSettingsActivity(MenuTag menuTag)
  {
    SettingsActivity.launch(this, menuTag, "");
  }

  public void launchFileListActivity()
  {
    FileBrowserHelper.openDirectoryPicker(this);
  }

  private void clearGameData(Context context)
  {
    int count = 0;
    String cachePath = DirectoryInitialization.getCacheDirectory();
    File dir = new File(cachePath);
    if (dir.exists())
    {
      for (File f : dir.listFiles())
      {
        if (f.getName().endsWith(".uidcache"))
        {
          if (f.delete())
          {
            count += 1;
          }
        }
      }
    }

    String shadersPath = cachePath + File.separator + "Shaders";
    dir = new File(shadersPath);
    if (dir.exists())
    {
      for (File f : dir.listFiles())
      {
        if (f.getName().endsWith(".cache"))
        {
          if (f.delete())
          {
            count += 1;
          }
        }
      }
    }

    Toast.makeText(context, context.getString(R.string.delete_cache_toast, count),
      Toast.LENGTH_SHORT).show();
  }

  public void launchOpenFileActivity()
  {
    FileBrowserHelper.openFilePicker(this, REQUEST_OPEN_FILE, true);
  }

  /**
   * @param requestCode An int describing whether the Activity that is returning did so successfully.
   * @param resultCode  An int describing what Activity is giving us this callback.
   * @param result      The information the returning Activity is providing us.
   */
  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent result)
  {
    switch (requestCode)
    {
      case REQUEST_ADD_DIRECTORY:
        // If the user picked a file, as opposed to just backing out.
        if (resultCode == MainActivity.RESULT_OK)
        {
          mDirToAdd = FileBrowserHelper.getSelectedDirectory(result);
        }
        break;

      case REQUEST_OPEN_FILE:
        // If the user picked a file, as opposed to just backing out.
        if (resultCode == MainActivity.RESULT_OK)
        {
          EmulationActivity.launchFile(this, FileBrowserHelper.getSelectedFiles(result));
        }
        break;
    }
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
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

  public void showGames()
  {
    mAdapter.swapDataSet(GameFileCacheService.getAllGameFiles());
  }
}
