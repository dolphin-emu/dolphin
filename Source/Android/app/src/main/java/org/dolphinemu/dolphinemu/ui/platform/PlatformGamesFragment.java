package org.dolphinemu.dolphinemu.ui.platform;

import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.application.injectors.FragmentInjector;
import org.dolphinemu.dolphinemu.ui.settings.BaseFragment;

import javax.inject.Inject;

public final class PlatformGamesFragment extends BaseFragment implements PlatformGamesView
{
	private static final String ARG_PLATFORM = BuildConfig.APPLICATION_ID + ".PLATFORM";

	@Inject
	public PlatformGamesPresenter mPresenter;

	private GameAdapter mAdapter;

	private FrameLayout mFrameContent;
	private RecyclerView mRecyclerView;

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

		mPresenter.onCreate(getArguments().getInt(ARG_PLATFORM));
	}

	@Nullable
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		View rootView = inflater.inflate(R.layout.fragment_grid, container, false);

		findViews(rootView);

		mPresenter.onCreateView();

		return rootView;
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState)
	{
		int columns = getResources().getInteger(R.integer.game_grid_columns);
		RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getActivity(), columns);
		mAdapter = new GameAdapter();

		mRecyclerView.setLayoutManager(layoutManager);
		mRecyclerView.setAdapter(mAdapter);

		mRecyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(8));
	}

	@Override
	public void refreshScreenshotAtPosition(int position)
	{
		mAdapter.notifyItemChanged(position);
	}

	@Override
	public void refresh()
	{
		mPresenter.refresh();
	}

	@Override
	public void onItemClick(String gameId)
	{
		// No-op for now
	}

	@Override
	public void showGames(Cursor games)
	{
		if (mAdapter != null)
		{
			mAdapter.swapCursor(games);
		}
	}

	@Override
	protected void inject()
	{
		FragmentInjector.inject(this);
	}

	@Override
	protected FrameLayout getContentLayout()
	{
		return mFrameContent;
	}

	@Override
	protected String getTitle()
	{
		return null;
	}

	@Override
	protected String getSubtitle()
	{
		return null;
	}

	private void findViews(View root)
	{
		mFrameContent = (FrameLayout) root.findViewById(R.id.frame_content);
		mRecyclerView = (RecyclerView) root.findViewById(R.id.grid_games);
	}
}
