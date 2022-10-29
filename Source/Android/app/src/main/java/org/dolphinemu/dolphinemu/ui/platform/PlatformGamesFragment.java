// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.color.MaterialColors;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;
import org.dolphinemu.dolphinemu.databinding.FragmentGamesBinding;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.main.MainView;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;

public final class PlatformGamesFragment extends Fragment implements PlatformGamesView
{
  private static final String ARG_PLATFORM = "platform";

  private GameAdapter mAdapter;

  private FragmentGamesBinding mBinding;

  public PlatformGamesFragment()
  {
    // Required empty constructor
  }

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
    mAdapter = new GameAdapter(requireActivity());
  }

  @NonNull
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
          Bundle savedInstanceState)
  {
    mBinding = FragmentGamesBinding.inflate(inflater, container, false);

    mAdapter.setStateRestorationPolicy(
            RecyclerView.Adapter.StateRestorationPolicy.PREVENT_WHEN_EMPTY);

    RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getActivity(),
            getResources().getInteger(R.integer.game_grid_columns));
    mBinding.gridGames.setLayoutManager(layoutManager);
    mBinding.gridGames.setAdapter(mAdapter);


    // Set theme color to the refresh animation's background
    mBinding.swipeRefresh.setProgressBackgroundColorSchemeColor(
            MaterialColors.getColor(mBinding.swipeRefresh, R.attr.colorPrimary));
    mBinding.swipeRefresh.setColorSchemeColors(
            MaterialColors.getColor(mBinding.swipeRefresh, R.attr.colorOnPrimary));

    mBinding.swipeRefresh.setOnRefreshListener(() ->
    {
      ((MainView) requireActivity()).setRefreshing(true);
      GameFileCacheManager.startRescan();
    });

    InsetsHelper.setUpList(getContext(), mBinding.gridGames);

    setRefreshing(GameFileCacheManager.isLoadingOrRescanning());
    showGames();

    return mBinding.getRoot();
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
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

  public void setRefreshing(boolean refreshing)
  {
    mBinding.swipeRefresh.setRefreshing(refreshing);
  }
}
