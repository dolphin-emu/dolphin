package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.FragmentManager;
import android.content.Intent;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v17.leanback.app.BrowseFragment;
import android.support.v17.leanback.database.CursorMapper;
import android.support.v17.leanback.widget.ArrayObjectAdapter;
import android.support.v17.leanback.widget.CursorObjectAdapter;
import android.support.v17.leanback.widget.HeaderItem;
import android.support.v17.leanback.widget.ListRow;
import android.support.v17.leanback.widget.ListRowPresenter;
import android.support.v17.leanback.widget.OnItemViewClickedListener;
import android.support.v17.leanback.widget.Presenter;
import android.support.v17.leanback.widget.Row;
import android.support.v17.leanback.widget.RowPresenter;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameRowPresenter;
import org.dolphinemu.dolphinemu.adapters.SettingsRowPresenter;
import org.dolphinemu.dolphinemu.model.Game;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.model.GameProvider;
import org.dolphinemu.dolphinemu.model.TvSettingsItem;
import org.dolphinemu.dolphinemu.ui.main.MainActivity;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.utils.StartupHandler;
import org.dolphinemu.dolphinemu.viewholders.TvGameViewHolder;

public final class TvMainActivity extends Activity
{
	protected BrowseFragment mBrowseFragment;

	private ArrayObjectAdapter mRowsAdapter;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_tv_main);

		final FragmentManager fragmentManager = getFragmentManager();
		mBrowseFragment = (BrowseFragment) fragmentManager.findFragmentById(
				R.id.fragment_game_list);

		// Set display parameters for the BrowseFragment
		mBrowseFragment.setHeadersState(BrowseFragment.HEADERS_ENABLED);
		mBrowseFragment.setTitle(getString(R.string.app_name));
		mBrowseFragment.setBadgeDrawable(getResources().getDrawable(
				R.drawable.ic_launcher, null));
		mBrowseFragment.setBrandColor(getResources().getColor(R.color.dolphin_blue_dark));

		buildRowsAdapter();

		mBrowseFragment.setOnItemViewClickedListener(
				new OnItemViewClickedListener()
				{
					@Override
					public void onItemClicked(Presenter.ViewHolder itemViewHolder, Object item, RowPresenter.ViewHolder rowViewHolder, Row row)
					{
						// Special case: user clicked on a settings row item.
						if (item instanceof TvSettingsItem)
						{
							TvSettingsItem settingsItem = (TvSettingsItem) item;

							switch (settingsItem.getItemId())
							{
								case R.id.menu_refresh:
									getContentResolver().insert(GameProvider.URI_REFRESH, null);

									// TODO Let the Activity know the data is refreshed in some other, better way.
									recreate();
									break;

								case R.id.menu_settings:
									// Launch the Settings Actvity.
									Intent settings = new Intent(TvMainActivity.this, SettingsActivity.class);
									startActivity(settings);
									break;

								case R.id.button_add_directory:
									Intent fileChooser = new Intent(TvMainActivity.this, AddDirectoryActivity.class);

									// The second argument to this method is read below in onActivityResult().
									startActivityForResult(fileChooser, MainPresenter.REQUEST_ADD_DIRECTORY);

									break;

								default:
									Toast.makeText(TvMainActivity.this, "Unimplemented menu option.", Toast.LENGTH_SHORT).show();
									break;
							}
						}
						else
						{
							TvGameViewHolder holder = (TvGameViewHolder) itemViewHolder;
							// Start the emulation activity and send the path of the clicked ISO to it.
							Intent intent = new Intent(TvMainActivity.this, EmulationActivity.class);

							intent.putExtra("SelectedGame", holder.path);
							intent.putExtra("SelectedTitle", holder.title);
							intent.putExtra("ScreenPath", holder.screenshotPath);

							ActivityOptions options = ActivityOptions.makeSceneTransitionAnimation(
									TvMainActivity.this,
									holder.imageScreenshot,
									"image_game_screenshot");

							startActivity(intent, options.toBundle());
						}
					}
				});

		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		if (savedInstanceState == null)
			StartupHandler.HandleInit(this);
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
		switch (requestCode)
		{
			case MainPresenter.REQUEST_ADD_DIRECTORY:
				// If the user picked a file, as opposed to just backing out.
				if (resultCode == RESULT_OK)
				{
					// Sanity check to make sure the Activity that just returned was the AddDirectoryActivity;
					// other activities might use this callback in the future (don't forget to change Javadoc!)
					if (requestCode == MainPresenter.REQUEST_ADD_DIRECTORY)
					{
						// TODO Let the Activity know the data is refreshed in some other, better way.
						recreate();
					}
				}
				break;
		}
	}

	private void buildRowsAdapter()
	{
		mRowsAdapter = new ArrayObjectAdapter(new ListRowPresenter());

		// For each platform
		for (int platformIndex = 0; platformIndex <= Game.PLATFORM_ALL; ++platformIndex)
		{
			ListRow row = buildGamesRow(platformIndex);

			// Add row to the adapter only if it is not empty.
			if (row != null)
			{
				mRowsAdapter.add(row);
			}
		}

		ListRow settingsRow = buildSettingsRow();
		mRowsAdapter.add(settingsRow);

		mBrowseFragment.setAdapter(mRowsAdapter);
	}

	private ListRow buildGamesRow(int platform)
	{
		// Create an adapter for this row.
		CursorObjectAdapter row = new CursorObjectAdapter(new GameRowPresenter());

		Cursor games;
		if (platform == Game.PLATFORM_ALL)
		{
			// Get all games.
			games = getContentResolver().query(
					GameProvider.URI_GAME,                        // URI of table to query
					null,                                        // Return all columns
					null,                                        // Return all games
					null,                                        // Return all games
					GameDatabase.KEY_GAME_TITLE + " asc"        // Sort by game name, ascending order
			);
		}
		else
		{
			// Get games for this particular platform.
			games = getContentResolver().query(
					GameProvider.URI_GAME,                        // URI of table to query
					null,                                        // Return all columns
					GameDatabase.KEY_GAME_PLATFORM + " = ?",    // Select by platform
					new String[]{Integer.toString(platform)},    // Platform id
					GameDatabase.KEY_GAME_TITLE + " asc"        // Sort by game name, ascending order
			);
		}

		// If cursor is empty, don't return a Row.
		if (!games.moveToFirst())
		{
			return null;
		}

		row.changeCursor(games);
		row.setMapper(new CursorMapper()
		{
			@Override
			protected void bindColumns(Cursor cursor)
			{
				// No-op? Not sure what this does.
			}

			@Override
			protected Object bind(Cursor cursor)
			{
				return Game.fromCursor(cursor);
			}
		});

		String headerName;
		switch (platform)
		{
			case Game.PLATFORM_GC:
				headerName = "GameCube Games";
				break;

			case Game.PLATFORM_WII:
				headerName = "Wii Games";
				break;

			case Game.PLATFORM_WII_WARE:
				headerName = "WiiWare";
				break;

			case Game.PLATFORM_ALL:
				headerName = "All Games";
				break;

			default:
				headerName = "Error";
				break;
		}

		// Create a header for this row.
		HeaderItem header = new HeaderItem(platform, headerName);

		// Create the row, passing it the filled adapter and the header, and give it to the master adapter.
		return new ListRow(header, row);
	}

	private ListRow buildSettingsRow()
	{
		ArrayObjectAdapter rowItems = new ArrayObjectAdapter(new SettingsRowPresenter());

		rowItems.add(new TvSettingsItem(R.id.menu_refresh,
				R.drawable.ic_refresh_tv,
				R.string.grid_menu_refresh));

		rowItems.add(new TvSettingsItem(R.id.menu_settings,
				R.drawable.ic_settings_tv,
				R.string.grid_menu_settings));

		rowItems.add(new TvSettingsItem(R.id.button_add_directory,
				R.drawable.ic_add_tv,
				R.string.add_directory_title));

		// Create a header for this row.
		HeaderItem header = new HeaderItem(R.string.settings, getString(R.string.settings));

		return new ListRow(header, rowItems);
	}
}
