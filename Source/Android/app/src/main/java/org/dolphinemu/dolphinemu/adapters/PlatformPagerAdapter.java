// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentPagerAdapter;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesFragment;

public class PlatformPagerAdapter extends FragmentPagerAdapter
{
  private SwipeRefreshLayout.OnRefreshListener mOnRefreshListener;

  public final static int[] TAB_ICONS =
          {
                  R.drawable.ic_gamecube,
                  R.drawable.ic_wii,
                  R.drawable.ic_folder
          };

  public PlatformPagerAdapter(FragmentManager fm,
          SwipeRefreshLayout.OnRefreshListener onRefreshListener)
  {
    super(fm, BEHAVIOR_RESUME_ONLY_CURRENT_FRAGMENT);
    mOnRefreshListener = onRefreshListener;
  }

  @NonNull
  @Override
  public Fragment getItem(int position)
  {
    Platform platform = Platform.fromPosition(position);

    PlatformGamesFragment fragment = PlatformGamesFragment.newInstance(platform);
    fragment.setOnRefreshListener(mOnRefreshListener);
    return fragment;
  }

  @Override
  public int getCount()
  {
    return TAB_ICONS.length;
  }
}
