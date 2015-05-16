package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageButton;
import android.widget.Toolbar;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;
import org.dolphinemu.dolphinemu.model.Game;
import org.dolphinemu.dolphinemu.model.GcGame;
import org.dolphinemu.dolphinemu.services.AssetCopyService;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * The main Activity of the Lollipop style UI. Shows a grid of games on tablets & landscape phones,
 * shows a list of games on portrait phones.
 */
public final class GameGridActivity extends Activity
{
	private static final int REQUEST_ADD_DIRECTORY = 1;

	private GameAdapter mAdapter;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_game_grid);

		Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar_game_list);
		setActionBar(toolbar);

		ImageButton buttonAddDirectory = (ImageButton) findViewById(R.id.button_add_directory);
		RecyclerView recyclerView = (RecyclerView) findViewById(R.id.grid_games);

		// use this setting to improve performance if you know that changes
		// in content do not change the layout size of the RecyclerView
		//mRecyclerView.setHasFixedSize(true);

		// Specifying the LayoutManager determines how the RecyclerView arranges views.
		RecyclerView.LayoutManager layoutManager = new GridLayoutManager(this,
				getResources().getInteger(R.integer.game_grid_columns));
		recyclerView.setLayoutManager(layoutManager);

		recyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(8));

		// Create an adapter that will relate the dataset to the views on-screen.
		mAdapter = new GameAdapter(getGameList());
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
			// Copy assets into appropriate locations.
			Intent copyAssets = new Intent(this, AssetCopyService.class);
			startService(copyAssets);
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
				// Get the path the user selected in AddDirectoryActivity.
				String path = result.getStringExtra(AddDirectoryActivity.KEY_CURRENT_PATH);

				// Store this path as a preference.
				// TODO Use SQLite instead.
				SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
				SharedPreferences.Editor editor = prefs.edit();

				editor.putString(AddDirectoryActivity.KEY_CURRENT_PATH, path);

				// Using commit, not apply, in order to block so the next method has the correct data to load.
				editor.commit();

				mAdapter.setGameList(getGameList());
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
		}

		return false;
	}

	// TODO Replace all of this with a SQLite database
	private ArrayList<Game> getGameList()
	{
		ArrayList<Game> gameList = new ArrayList<Game>();

		final String DefaultDir = Environment.getExternalStorageDirectory() + File.separator + "dolphin-emu";

		NativeLibrary.SetUserDirectory(DefaultDir);

		// Extensions to filter by.
		Set<String> exts = new HashSet<String>(Arrays.asList(".dff", ".dol", ".elf", ".gcm", ".gcz", ".iso", ".wad", ".wbfs"));

		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());

		String path = prefs.getString(AddDirectoryActivity.KEY_CURRENT_PATH, "/");

		File currentDir = new File(path);
		File[] dirs = currentDir.listFiles();
		try
		{
			for (File entry : dirs)
			{
				if (!entry.isHidden() && !entry.isDirectory())
				{
					String entryName = entry.getName();

					// Check that the file has an appropriate extension before trying to read out of it.
					if (exts.contains(entryName.toLowerCase().substring(entryName.lastIndexOf('.'))))
					{
						GcGame game = new GcGame(NativeLibrary.GetTitle(entry.getAbsolutePath()),
								NativeLibrary.GetDescription(entry.getAbsolutePath()).replace("\n", " "),
								NativeLibrary.GetCountry(entry.getAbsolutePath()),
								entry.getAbsolutePath(),
								NativeLibrary.GetGameId(entry.getAbsolutePath()),
								NativeLibrary.GetDate(entry.getAbsolutePath()));

						gameList.add(game);
					}

				}

			}
		}
		catch (Exception ignored)
		{

		}

		return gameList;
	}
}
