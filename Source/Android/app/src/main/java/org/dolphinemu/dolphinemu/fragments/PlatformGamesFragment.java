package org.dolphinemu.dolphinemu.fragments;

import android.app.Fragment;
import android.app.LoaderManager;
import android.content.Loader;
import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;

public class PlatformGamesFragment extends Fragment
{
	private static final String ARG_PLATFORM = BuildConfig.APPLICATION_ID + ".PLATFORM";

	private int mPlatform;

	private GameAdapter mAdapter;

	public static PlatformGamesFragment newInstance(int platform)
	{
		PlatformGamesFragment fragment = new PlatformGamesFragment();

		Bundle args = new Bundle();
		args.putInt(ARG_PLATFORM, platform);

		fragment.setArguments(args);
		return fragment;
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		mPlatform = getArguments().getInt(ARG_PLATFORM);
	}

	@Nullable
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		View rootView = inflater.inflate(R.layout.fragment_grid, container, false);
		RecyclerView recyclerView = (RecyclerView) rootView.findViewById(R.id.grid_games);

		// Specifying the LayoutManager determines how the RecyclerView arranges views.

		RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getActivity(),
				getResources().getInteger(R.integer.game_grid_columns));
		recyclerView.setLayoutManager(layoutManager);
		recyclerView.setItemAnimator(new DefaultItemAnimator()
		{
			@Override
			public boolean animateChange(RecyclerView.ViewHolder oldHolder, RecyclerView.ViewHolder newHolder, int fromX, int fromY, int toX, int toY)
			{
				dispatchChangeFinished(newHolder, false);
				return true;
			}
		});

		recyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(8));

		// Create an adapter that will relate the dataset to the views on-screen. +1 because of LOADER_ID_ALL
		getLoaderManager().initLoader(mPlatform, null,
				(LoaderManager.LoaderCallbacks<Cursor>) getActivity());

		mAdapter = new GameAdapter();
		recyclerView.setAdapter(mAdapter);

		return rootView;
	}

	public void refreshScreenshotAtPosition(int position)
	{
		mAdapter.notifyItemChanged(position);
	}

	public void refresh()
	{
		Log.d("DolphinEmu", "[PlatformGamesFragment] " + mPlatform + ": Refreshing...");
		// +1 because of LOADER_ID_ALL
		getLoaderManager().restartLoader(mPlatform, null, (LoaderManager.LoaderCallbacks<Cursor>) getActivity());
	}

	public void onLoadFinished(Loader<Cursor> loader, Cursor data)
	{
		// TODO Play some sort of load-finished animation; maybe fade the list in.

		Log.d("DolphinEmu", "[PlatformGamesFragment] " + mPlatform + ": Load finished, swapping cursor...");
		Log.d("DolphinEmu", "[PlatformGamesFragment] " + mPlatform + ": Cursor size: " + data.getCount());
		if (mAdapter != null)
		{
			mAdapter.swapCursor(data);
		}
		else
		{
			Log.e("DolphinEmu", "[PlatformGamesFragment] " + mPlatform + ": No adapter available.");
		}
	}

	public void onLoaderReset()
	{
		Log.e("DolphinEmu", "[PlatformGamesFragment] " + mPlatform + ": Loader reset; clearing data from view.");
		if (mAdapter != null)
		{
			mAdapter.swapCursor(null);
		}
	}
}
