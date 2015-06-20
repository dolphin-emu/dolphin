package org.dolphinemu.dolphinemu.activities;

import android.app.LoaderManager;
import android.content.CursorLoader;
import android.content.Intent;
import android.content.Loader;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.design.widget.FloatingActionButton;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toolbar;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.model.GameProvider;
import org.dolphinemu.dolphinemu.services.AssetCopyService;

/**
 * The main Activity of the Lollipop style UI. Shows a grid of games on tablets & landscape phones,
 * shows a list of games on portrait phones.
 */
public final class GameGridActivity extends AppCompatActivity implements LoaderManager.LoaderCallbacks<Cursor>
{
	private static final int REQUEST_ADD_DIRECTORY = 1;

	private static final int LOADER_ID_GAMES = 1;
	// TODO When each platform has its own tab, there should be a LOADER_ID for each platform.

	private GameAdapter mAdapter;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_game_grid);

		Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar_game_list);
		setActionBar(toolbar);

		FloatingActionButton buttonAddDirectory = (FloatingActionButton) findViewById(R.id.button_add_directory);
		RecyclerView recyclerView = (RecyclerView) findViewById(R.id.grid_games);

		// TODO Rather than calling into native code, this should use the commented line below.
		// String versionName = BuildConfig.VERSION_NAME;
		String versionName = NativeLibrary.GetVersionString();
		toolbar.setSubtitle(versionName);

		// Specifying the LayoutManager determines how the RecyclerView arranges views.
		RecyclerView.LayoutManager layoutManager = new GridLayoutManager(this,
				getResources().getInteger(R.integer.game_grid_columns));
		recyclerView.setLayoutManager(layoutManager);

		recyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(8));

		// Create an adapter that will relate the dataset to the views on-screen.
		getLoaderManager().initLoader(LOADER_ID_GAMES, null, this);
		mAdapter = new GameAdapter();
		recyclerView.setAdapter(mAdapter);

		buttonAddDirectory.setOnClickListener(new View.OnClickListener()
		{
			@Override
			public void onClick(View view)
			{
				Intent fileChooser = new Intent(GameGridActivity.this, AddDirectoryActivity.class);

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
				getLoaderManager().restartLoader(LOADER_ID_GAMES, null, this);
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
				getLoaderManager().restartLoader(LOADER_ID_GAMES, null, this);

				return true;
		}

		return false;
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
			case LOADER_ID_GAMES:
				// TODO Play some sort of load-starting animation; maybe fade the list out.

				return new CursorLoader(
						this,                           // Parent activity context
						GameProvider.URI_GAME,          // URI of table to query
						null,                           // Return all columns
						null,                           // No selection clause
						null,                           // No selection arguments
						GameDatabase.KEY_GAME_TITLE + " asc"   // Sort by game name, ascending order
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

		// TODO When each platform has its own tab, this should just call into those tabs instead.
		switch (id)
		{
			case LOADER_ID_GAMES:
				mAdapter.swapCursor(data);
				// TODO Play some sort of load-finished animation; maybe fade the list in.
				break;

			default:
				Log.e("DolphinEmu", "Bad ID passed in.");
		}

	}

	@Override
	public void onLoaderReset(Loader<Cursor> loader)
	{
		Log.d("DolphinEmu", "Loader resetting.");

		// TODO ¯\_(ツ)_/¯
	}
}
