/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.settings.cpu.CPUSettingsFragment;
import org.dolphinemu.dolphinemu.settings.input.InputConfigFragment;
import org.dolphinemu.dolphinemu.settings.video.VideoSettingsFragment;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v13.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;

/**
 * Main activity that manages all of the preference fragments used to display
 * the settings to the user.
 */
public final class PrefsActivity extends Activity implements OnSharedPreferenceChangeListener
{
	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.viewpager);

		// Set the ViewPager adapter.
		final ViewPager viewPager = (ViewPager) findViewById(R.id.pager);
		viewPager.setAdapter(new ViewPagerAdapter(getFragmentManager()));

		// Register the preference change listener.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(this);
		sPrefs.registerOnSharedPreferenceChangeListener(this);
	}

	@Override
	public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key)
	{
		// If any change is made to the preferences in the front-end, immediately save them.
		UserPreferences.SavePrefsToIni(this);
	}

	/**
	 * A {@link FragmentPagerAdapter} that returns a fragment 
	 * corresponding to one of the sections/tabs/pages.
	 */
	private final class ViewPagerAdapter extends FragmentPagerAdapter
	{
		private final String[] pageTitles = {
			getString(R.string.cpu_settings),
			getString(R.string.input_settings),
			getString(R.string.video_settings)
		};

		public ViewPagerAdapter(FragmentManager fm)
		{
			super(fm);
		}

		@Override
		public Fragment getItem(int position)
		{
			switch(position)
			{
				case 0:
					return new CPUSettingsFragment();

				case 1:
					return new InputConfigFragment();

				case 2:
					return new VideoSettingsFragment();

				default: // Should never happen.
					return null;
			}
		}

		@Override
		public int getCount()
		{
			// Show total pages.
			return 3;
		}

		@Override
		public CharSequence getPageTitle(int position)
		{
			return pageTitles[position];
		}
	}
}
