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

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GamePresenter;
import org.dolphinemu.dolphinemu.model.Game;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.model.GameProvider;
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
				});
	}

	private void buildRowsAdapter()
	{
		mRowsAdapter = new ArrayObjectAdapter(new ListRowPresenter());

		// For each row
		for (int platformIndex = 0; platformIndex <= Game.PLATFORM_WII_WARE; ++platformIndex)
		{
			// Create an adapter for this row.
			CursorObjectAdapter row = new CursorObjectAdapter(new GamePresenter(platformIndex));

			// Add items to the adapter.
			Cursor games = getContentResolver().query(
					GameProvider.URI_GAME,                        // URI of table to query
					null,                                        // Return all columns
					GameDatabase.KEY_GAME_PLATFORM + " = ?",    // Select by platform
					new String[]{Integer.toString(platformIndex)},    // Platform id
					GameDatabase.KEY_GAME_TITLE + " asc"        // Sort by game name, ascending order
			);

			row.swapCursor(games);
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
			switch (platformIndex)
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

				default:
					headerName = "Error";
					break;
			}

			// Create a header for this row.
			HeaderItem header = new HeaderItem(platformIndex, headerName);

			// Create the row, passing it the filled adapter and the header, and give it to the master adapter.
			mRowsAdapter.add(new ListRow(header, row));
		}

		mBrowseFragment.setAdapter(mRowsAdapter);
	}
}
