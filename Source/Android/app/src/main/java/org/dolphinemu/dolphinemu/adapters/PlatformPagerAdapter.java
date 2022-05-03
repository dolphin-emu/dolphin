// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;
import androidx.viewpager2.adapter.FragmentStateAdapter;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesFragment;

public class PlatformPagerAdapter extends FragmentStateAdapter
{
  private SwipeRefreshLayout.OnRefreshListener mOnRefreshListener;

  public final static int[] TAB_ICONS =
          {
                  R.drawable.ic_gamecube,
                  R.drawable.ic_wii,
                  R.drawable.ic_folder
          };

  public PlatformPagerAdapter(FragmentActivity fa,
          SwipeRefreshLayout.OnRefreshListener onRefreshListener)
  {
    super(fa);
    mOnRefreshListener = onRefreshListener;
  }

  @NonNull
  @Override
  public Fragment createFragment(int position)
  {
    Platform platform = Platform.fromPosition(position);

    PlatformGamesFragment fragment = PlatformGamesFragment.newInstance(platform);
    fragment.setOnRefreshListener(mOnRefreshListener);
    return fragment;
  }

  @Override
  public int getItemCount()
  {
    return TAB_ICONS.length;
  }
}
