package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.widget.Toolbar;

import org.dolphinemu.dolphinemu.AssetCopyService;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;
import org.dolphinemu.dolphinemu.model.Game;
import org.dolphinemu.dolphinemu.model.GcGame;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class GameGridActivity extends Activity
{
	private RecyclerView mRecyclerView;
	private RecyclerView.Adapter mAdapter;
	private RecyclerView.LayoutManager mLayoutManager;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_game_grid);

		Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar_game_list);
		setActionBar(toolbar);

		mRecyclerView = (RecyclerView) findViewById(R.id.grid_games);

		// use this setting to improve performance if you know that changes
		// in content do not change the layout size of the RecyclerView
		//mRecyclerView.setHasFixedSize(true);

		// Specifying the LayoutManager determines how the RecyclerView arranges views.
		mLayoutManager = new GridLayoutManager(this, 4);
		mRecyclerView.setLayoutManager(mLayoutManager);

		mRecyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(8));

		// Create an adapter that will relate the dataset to the views on-screen.
		mAdapter = new GameAdapter(getGameList());
		mRecyclerView.setAdapter(mAdapter);

		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		if (savedInstanceState == null)
		{
			// Copy assets into appropriate locations.
			Intent copyAssets = new Intent(this, AssetCopyService.class);
			startService(copyAssets);
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.gamelist_menu, menu);
		return true;

	}

	// TODO Replace all of this with a SQLite database
	private ArrayList<Game> getGameList()
	{
		ArrayList<Game> gameList = new ArrayList<Game>();

		final String DefaultDir = Environment.getExternalStorageDirectory() + File.separator + "dolphin-emu";

		NativeLibrary.SetUserDirectory(DefaultDir);

		String Directories = NativeLibrary.GetConfig("Dolphin.ini", "General", "ISOPaths", "0");
		Log.v("DolphinEmu", "Directories: " + Directories);
		int intDirectories = Integer.parseInt(Directories);

		// Extensions to filter by.
		Set<String> exts = new HashSet<String>(Arrays.asList(".dff", ".dol", ".elf", ".gcm", ".gcz", ".iso", ".wad", ".wbfs"));

		for (int a = 0; a < intDirectories; ++a)
		{
			String BrowseDir = NativeLibrary.GetConfig("Dolphin.ini", "General", "ISOPath" + a, "");
			Log.v("DolphinEmu", "Directory " + a + ": " + BrowseDir);

			File currentDir = new File(BrowseDir);
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
									// TODO Some games might actually not be from this region, believe it or not.
									"United States",
									entry.getAbsolutePath(),
									NativeLibrary.GetGameId(entry.getAbsolutePath()),
									NativeLibrary.GetDate(entry.getAbsolutePath()));

							gameList.add(game);
						}

					}

				}
			} catch (Exception ignored)
			{
			}
		}

		return gameList;
	}
}
