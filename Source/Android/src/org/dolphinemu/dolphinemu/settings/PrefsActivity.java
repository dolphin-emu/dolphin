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

import android.app.ActionBar;
import android.app.ActionBar.Tab;
import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
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
public final class PrefsActivity extends Activity implements ActionBar.TabListener, OnSharedPreferenceChangeListener
{
	/**
	 * The {@link ViewPager} that will host the section contents.
	 */
	private ViewPager mViewPager;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Set the ViewPager.
		setContentView(R.layout.prefs_viewpager);
		mViewPager = (ViewPager) findViewById(R.id.pager);

		// Set the ViewPager adapter.
		final ViewPagerAdapter mSectionsPagerAdapter = new ViewPagerAdapter(getFragmentManager());
		mViewPager.setAdapter(mSectionsPagerAdapter);

		// Register the preference change listener.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(this);
		sPrefs.registerOnSharedPreferenceChangeListener(this);

		// Set up the action bar.
		final ActionBar actionBar = getActionBar();
		actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
		actionBar.addTab(actionBar.newTab().setText(R.string.cpu_settings).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.input_settings).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.video_settings).setTabListener(this));

		// When swiping between different sections, select the corresponding
		// tab. We can also use ActionBar.Tab#select() to do this if we have
		// a reference to the Tab.
		mViewPager.setOnPageChangeListener(new ViewPager.SimpleOnPageChangeListener()
		{
			@Override
			public void onPageSelected(int position)
			{
				actionBar.setSelectedNavigationItem(position);
			}
		} );
	}

	public void onTabSelected(Tab tab, FragmentTransaction ft)
	{
		// When the given tab is selected, switch to the corresponding page in the ViewPager.
		mViewPager.setCurrentItem(tab.getPosition());
	}

	public void onTabReselected(Tab tab, FragmentTransaction ft)
	{
		// Do nothing.
	}

	public void onTabUnselected(Tab tab, FragmentTransaction ft)
	{
		// Do nothing.
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
			switch(position)
			{
				case 0:
					return getString(R.string.cpu_settings);

				case 1:
					return getString(R.string.input_settings);

				case 2:
					return getString(R.string.video_settings);

				default: // Should never happen.
					return null;
			}
		}
	}
}
