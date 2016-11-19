package org.dolphinemu.dolphinemu.adapters;

import android.app.Fragment;
import android.app.FragmentManager;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v13.app.FragmentPagerAdapter;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ImageSpan;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesFragment;

public class PlatformPagerAdapter extends FragmentPagerAdapter
{
	private Context mContext;

	private final static int[] TAB_ICONS =
	{
			R.drawable.ic_gamecube,
			R.drawable.ic_wii,
			R.drawable.ic_folder // WiiWare TODO Have an icon here.
	};

	public PlatformPagerAdapter(FragmentManager fm, Context context)
	{
		super(fm);
		mContext = context;
	}

	@Override
	public Fragment getItem(int position)
	{
		return PlatformGamesFragment.newInstance(position);
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
		// TODO This workaround will eventually not be necessary; switch to more legit methods when that is the case
		// TODO Also remove additional hax from styles.xml
		Drawable drawable = mContext.getResources().getDrawable(TAB_ICONS[position]);
		drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());

		ImageSpan imageSpan = new ImageSpan(drawable, ImageSpan.ALIGN_BOTTOM);

		SpannableString sb = new SpannableString(" ");
		sb.setSpan(imageSpan, 0, 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

		return sb;
	}
}
