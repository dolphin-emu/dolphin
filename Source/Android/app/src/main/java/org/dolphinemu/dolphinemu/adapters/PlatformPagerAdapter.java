// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ImageSpan;

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
  private Context mContext;
  private SwipeRefreshLayout.OnRefreshListener mOnRefreshListener;

  private final static String[] TAB_TEXTS = { "NGC", "Wii", "Ware" };

  public PlatformPagerAdapter(FragmentManager fm, Context context,
          SwipeRefreshLayout.OnRefreshListener onRefreshListener)
  {
    super(fm, BEHAVIOR_RESUME_ONLY_CURRENT_FRAGMENT);
    mContext = context;
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
    return TAB_TEXTS.length;
  }

  @Override
  public CharSequence getPageTitle(int position)
  {
    return TAB_TEXTS[position];
  }
}
