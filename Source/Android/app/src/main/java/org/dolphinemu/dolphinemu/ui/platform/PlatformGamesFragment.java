// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform;

import android.os.Bundle;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;

public final class PlatformGamesFragment extends Fragment implements PlatformGamesView
{
  private static final String ARG_PLATFORM = "platform";

  private GameAdapter mAdapter;
  private RecyclerView mRecyclerView;
  private SwipeRefreshLayout mSwipeRefresh;
  private SwipeRefreshLayout.OnRefreshListener mOnRefreshListener;

  public static PlatformGamesFragment newInstance(Platform platform)
  {
    PlatformGamesFragment fragment = new PlatformGamesFragment();

    Bundle args = new Bundle();
    args.putSerializable(ARG_PLATFORM, platform);

    fragment.setArguments(args);
    return fragment;
  }

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    View rootView = inflater.inflate(R.layout.fragment_grid, container, false);

    findViews(rootView);

    return rootView;
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState)
  {
    int columns = getResources().getInteger(R.integer.game_grid_columns);
    RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getActivity(), columns);
    mAdapter = new GameAdapter();

    TypedValue typedValue = new TypedValue();
    requireActivity().getTheme().resolveAttribute(R.attr.colorPrimary, typedValue, true);
    mSwipeRefresh.setColorSchemeColors(typedValue.data);

    mSwipeRefresh.setOnRefreshListener(mOnRefreshListener);

    mRecyclerView.setLayoutManager(layoutManager);
    mRecyclerView.setAdapter(mAdapter);

    mRecyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(8));

    setRefreshing(GameFileCacheManager.isLoadingOrRescanning());

    showGames();
  }

  @Override
  public void refreshScreenshotAtPosition(int position)
  {
    mAdapter.notifyItemChanged(position);
  }

  @Override
  public void onItemClick(String gameId)
  {
    // No-op for now
  }

  @Override
  public void showGames()
  {
    if (mAdapter != null)
    {
      Platform platform = (Platform) getArguments().getSerializable(ARG_PLATFORM);
      mAdapter.swapDataSet(GameFileCacheManager.getGameFilesForPlatform(platform));
    }
  }

  @Override
  public void refetchMetadata()
  {
    mAdapter.refetchMetadata();
  }

  public void setOnRefreshListener(@Nullable SwipeRefreshLayout.OnRefreshListener listener)
  {
    mOnRefreshListener = listener;

    if (mSwipeRefresh != null)
      mSwipeRefresh.setOnRefreshListener(listener);
  }

  public void setRefreshing(boolean refreshing)
  {
    mSwipeRefresh.setRefreshing(refreshing);
  }

  private void findViews(View root)
  {
    mSwipeRefresh = root.findViewById(R.id.swipe_refresh);
    mRecyclerView = root.findViewById(R.id.grid_games);
  }
}
