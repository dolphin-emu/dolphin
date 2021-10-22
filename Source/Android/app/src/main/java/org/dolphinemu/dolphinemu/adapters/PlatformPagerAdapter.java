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

  private final static int[] TAB_ICONS =
          {
                  R.drawable.ic_gamecube,
                  R.drawable.ic_wii,
                  R.drawable.ic_folder
          };

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
    return TAB_ICONS.length;
  }

  @Override
  public CharSequence getPageTitle(int position)
  {
    // Hax from https://guides.codepath.com/android/Google-Play-Style-Tabs-using-TabLayout#design-support-library
    // Apparently a workaround for TabLayout not supporting icons.
    // TODO: This workaround will eventually not be necessary; switch to more legit methods when that is the case
    // TODO: Also remove additional hax from styles.xml
    Drawable drawable = mContext.getResources().getDrawable(TAB_ICONS[position]);
    drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());

    ImageSpan imageSpan = new ImageSpan(drawable, ImageSpan.ALIGN_BOTTOM);

    SpannableString sb = new SpannableString(" ");
    sb.setSpan(imageSpan, 0, 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

    return sb;
  }
}
