// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.RecyclerView;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.google.android.material.color.MaterialColors;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;
import org.dolphinemu.dolphinemu.databinding.FragmentGridBinding;
import org.dolphinemu.dolphinemu.layout.AutofitGridLayoutManager;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;

public final class PlatformGamesFragment extends Fragment implements PlatformGamesView
{
  private static final String ARG_PLATFORM = "platform";

  private SwipeRefreshLayout mSwipeRefresh;
  private SwipeRefreshLayout.OnRefreshListener mOnRefreshListener;

  private FragmentGridBinding mBinding;

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

  @NonNull
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
          Bundle savedInstanceState)
  {
    mBinding = FragmentGridBinding.inflate(inflater, container, false);
    return mBinding.getRoot();
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState)
  {
    mSwipeRefresh = mBinding.swipeRefresh;
    GameAdapter adapter = new GameAdapter(requireActivity());
    adapter.setStateRestorationPolicy(
            RecyclerView.Adapter.StateRestorationPolicy.PREVENT_WHEN_EMPTY);
    mBinding.gridGames.setAdapter(adapter);
    mBinding.gridGames.setLayoutManager(new AutofitGridLayoutManager(requireContext(),
            getResources().getDimensionPixelSize(R.dimen.card_width)));

    // Set theme color to the refresh animation's background
    mSwipeRefresh.setProgressBackgroundColorSchemeColor(
            MaterialColors.getColor(mSwipeRefresh, R.attr.colorPrimary));
    mSwipeRefresh.setColorSchemeColors(
            MaterialColors.getColor(mSwipeRefresh, R.attr.colorOnPrimary));

    mSwipeRefresh.setOnRefreshListener(mOnRefreshListener);

    setInsets();

    setRefreshing(GameFileCacheManager.isLoadingOrRescanning());

    showGames();
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }

  @Override
  public void onItemClick(String gameId)
  {
    // No-op for now
  }

  @Override
  public void showGames()
  {
    if (mBinding == null)
      return;

    if (mBinding.gridGames.getAdapter() != null)
    {
      Platform platform = (Platform) getArguments().getSerializable(ARG_PLATFORM);
      ((GameAdapter) mBinding.gridGames.getAdapter()).swapDataSet(
              GameFileCacheManager.getGameFilesForPlatform(platform));
    }
  }

  @Override
  public void refetchMetadata()
  {
    ((GameAdapter) mBinding.gridGames.getAdapter()).refetchMetadata();
  }

  public void setOnRefreshListener(@Nullable SwipeRefreshLayout.OnRefreshListener listener)
  {
    mOnRefreshListener = listener;

    if (mSwipeRefresh != null)
    {
      mSwipeRefresh.setOnRefreshListener(listener);
    }
  }

  public void setRefreshing(boolean refreshing)
  {
    mBinding.swipeRefresh.setRefreshing(refreshing);
  }

  private void setInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mBinding.gridGames, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
      v.setPadding(0, 0, 0,
              insets.bottom + getResources().getDimensionPixelSize(R.dimen.spacing_list) +
                      getResources().getDimensionPixelSize(R.dimen.spacing_fab));
      return windowInsets;
    });
  }
}
