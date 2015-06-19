package org.dolphinemu.dolphinemu.activities;

import android.app.LoaderManager;
import android.content.CursorLoader;
import android.content.Intent;
import android.content.Loader;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.Nullable;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.TabLayout;
import android.support.v4.view.ViewPager;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.PlatformPagerAdapter;
import org.dolphinemu.dolphinemu.fragments.PlatformGamesFragment;
import org.dolphinemu.dolphinemu.model.Game;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.model.GameProvider;
import org.dolphinemu.dolphinemu.services.AssetCopyService;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity implements LoaderManager.LoaderCallbacks<Cursor>
{
	private static final int REQUEST_ADD_DIRECTORY = 1;

	/**
	 * It is important to keep track of loader ID separately from platform ID (see Game.java)
	 * because we could potentially have Loaders that load things other than Games.
	 */
	public static final int LOADER_ID_ALL = 100; // TODO
	public static final int LOADER_ID_GAMECUBE = 0;
	public static final int LOADER_ID_WII = 1;
	public static final int LOADER_ID_WIIWARE = 2;

	private ViewPager mViewPager;
	private PlatformPagerAdapter mPlatformPagerAdapter;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		// Set up the Toolbar.
		Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar_main);
		setSupportActionBar(toolbar);

		// TODO Rather than calling into native code, this should use the commented line below.
		// String versionName = BuildConfig.VERSION_NAME;
		String versionName = NativeLibrary.GetVersionString();
		toolbar.setSubtitle(versionName);

		// Set up the Tab bar.
		mViewPager = (ViewPager) findViewById(R.id.pager_platforms);

		mPlatformPagerAdapter = new PlatformPagerAdapter(getFragmentManager(), this);
		mViewPager.setAdapter(mPlatformPagerAdapter);

		TabLayout tabLayout = (TabLayout) findViewById(R.id.tabs_platforms);
		tabLayout.setupWithViewPager(mViewPager);

		// Set up the FAB.
		FloatingActionButton buttonAddDirectory = (FloatingActionButton) findViewById(R.id.button_add_directory);
		buttonAddDirectory.setOnClickListener(new View.OnClickListener()
		{
			@Override
			public void onClick(View view)
			{
				Intent fileChooser = new Intent(MainActivity.this, AddDirectoryActivity.class);

				// The second argument to this method is read below in onActivityResult().
				startActivityForResult(fileChooser, REQUEST_ADD_DIRECTORY);
			}
		});

		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		if (savedInstanceState == null)
		{
			NativeLibrary.SetUserDirectory(""); // Auto-Detect

			SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
			boolean assetsCopied = preferences.getBoolean("assetsCopied", false);

			// Only perform these extensive copy operations once.
			if (!assetsCopied)
			{
				// Copy assets into appropriate locations.
				Intent copyAssets = new Intent(this, AssetCopyService.class);
				startService(copyAssets);
			}
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
		// If the user picked a file, as opposed to just backing out.
		if (resultCode == RESULT_OK)
		{
			// Sanity check to make sure the Activity that just returned was the AddDirectoryActivity;
			// other activities might use this callback in the future (don't forget to change Javadoc!)
			if (requestCode == REQUEST_ADD_DIRECTORY)
			{
				refreshFragment();
			}
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.menu_game_grid, menu);
		return true;
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
		switch (item.getItemId())
		{
			case R.id.menu_settings:
				// Launch the Settings Actvity.
				Intent settings = new Intent(this, SettingsActivity.class);
				startActivity(settings);
				return true;

			case R.id.menu_refresh:
				getContentResolver().insert(GameProvider.URI_REFRESH, null);
				refreshFragment();

				return true;
		}

		return false;
	}

	public void refreshFragment()
	{
		PlatformGamesFragment fragment = getPlatformFragment(mViewPager.getCurrentItem());
		if (fragment != null)
		{
			fragment.refresh();
		}
	}


	/**
	 * Callback that's invoked when the system has initialized the Loader and
	 * is ready to start the query. This usually happens when initLoader() is
	 * called. Here, we use it to make a DB query for games.
	 *
	 * @param id   The ID value passed to the initLoader() call that triggered this.
	 * @param args The args bundle supplied by the caller.
	 * @return A new Loader instance that is ready to start loading.
	 */
	@Override
	public Loader<Cursor> onCreateLoader(int id, Bundle args)
	{
		Log.d("DolphinEmu", "Creating loader with id: " + id);

		// Take action based on the ID of the Loader that's being created.
		switch (id)
		{
			case LOADER_ID_ALL:
				// TODO Play some sort of load-starting animation; maybe fade the list out.

				return new CursorLoader(
						this,                                    // Parent activity context
						GameProvider.URI_GAME,                    // URI of table to query
						null,                                    // Return all columns
						null,                                    // No selection clause
						null,                                    // No selection arguments
						GameDatabase.KEY_GAME_TITLE + " asc"    // Sort by game name, ascending order
				);

			case LOADER_ID_GAMECUBE:
			case LOADER_ID_WII:
			case LOADER_ID_WIIWARE:
				// TODO Play some sort of load-starting animation; maybe fade the list out.

				return new CursorLoader(
						this,                                        // Parent activity context
						GameProvider.URI_GAME,                        // URI of table to query
						null,                                        // Return all columns
						GameDatabase.KEY_GAME_PLATFORM + " = ?",    // Select by platform
						new String[]{Integer.toString(id)},    // Platform id is Loader id minus 1
						GameDatabase.KEY_GAME_TITLE + " asc"        // Sort by game name, ascending order
				);

			default:
				Log.e("DolphinEmu", "Bad ID passed in.");
				return null;
		}
	}

	/**
	 * Callback that's invoked when the Loader returned in onCreateLoader is finished
	 * with its task. In this case, the game DB query is finished, so we should put the results
	 * on screen.
	 *
	 * @param loader The loader that finished.
	 * @param data   The data the Loader loaded.
	 */
	@Override
	public void onLoadFinished(Loader<Cursor> loader, Cursor data)
	{
		int id = loader.getId();
		Log.d("DolphinEmu", "Loader finished with id: " + id);

		PlatformGamesFragment fragment = null;
		switch (id)
		{
			case LOADER_ID_GAMECUBE:
				fragment = getPlatformFragment(Game.PLATFORM_GC);
				break;

			case LOADER_ID_WII:
				fragment = getPlatformFragment(Game.PLATFORM_WII);
				break;

			case LOADER_ID_WIIWARE:
				fragment = getPlatformFragment(Game.PLATFORM_WII_WARE);
				break;

			// TODO case LOADER_ID_ALL:

			default:
				Log.e("DolphinEmu", "Bad ID passed in.");
				break;
		}

		if (fragment != null)
		{
			fragment.onLoadFinished(loader, data);
		}
	}

	@Override
	public void onLoaderReset(Loader<Cursor> loader)
	{
		int id = loader.getId();
		Log.e("DolphinEmu", "Loader resetting with id: " + id);

		PlatformGamesFragment fragment = null;
		switch (id)
		{
			case LOADER_ID_GAMECUBE:
				fragment = getPlatformFragment(Game.PLATFORM_GC);
				break;

			case LOADER_ID_WII:
				fragment = getPlatformFragment(Game.PLATFORM_WII);
				break;

			case LOADER_ID_WIIWARE:
				fragment = getPlatformFragment(Game.PLATFORM_WII_WARE);
				break;

			// TODO case LOADER_ID_ALL:

			default:
				Log.e("DolphinEmu", "Bad ID passed in.");
				break;
		}

		if (fragment != null)
		{
			fragment.onLoaderReset();
		}
	}

	@Nullable
	public PlatformGamesFragment getPlatformFragment(int platform)
	{
		String fragmentTag = "android:switcher:" + mViewPager.getId() + ":" + platform;

		return (PlatformGamesFragment) getFragmentManager().findFragmentByTag(fragmentTag);
	}
}
