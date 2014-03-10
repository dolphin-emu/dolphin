/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

import android.app.ActionBar;
import android.app.ActionBar.Tab;
import android.app.ActionBar.TabListener;
import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.os.Bundle;
import android.support.v13.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;


/**
 * Activity for the about menu, which displays info
 * related to the CPU and GPU. Along with misc other info.
 */
public final class AboutActivity extends Activity implements TabListener
{
	private ViewPager viewPager;
	private final EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Set the view pager
		setContentView(R.layout.viewpager);
		viewPager = (ViewPager) findViewById(R.id.pager);

		// Initialize the ViewPager adapter.
		final ViewPagerAdapter adapter = new ViewPagerAdapter(getFragmentManager());
		viewPager.setAdapter(adapter);

		// Set up the ActionBar
		final ActionBar actionBar = getActionBar();
		actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
		actionBar.addTab(actionBar.newTab().setText(R.string.general).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.cpu).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.gles_two).setTabListener(this));
		if (eglHelper.supportsGLES3())
			actionBar.addTab(actionBar.newTab().setText(R.string.gles_three).setTabListener(this));
		if (eglHelper.supportsOpenGL())
			actionBar.addTab(actionBar.newTab().setText(R.string.desktop_gl).setTabListener(this));

		// Set the page change listener
		viewPager.setOnPageChangeListener(new ViewPager.SimpleOnPageChangeListener()
		{
			@Override
			public void onPageSelected(int position)
			{
				actionBar.setSelectedNavigationItem(position);
			}
		});
	}

	@Override
	public void onDestroy()
	{
		super.onDestroy();

		eglHelper.closeHelper();
	}

	@Override
	public void onTabSelected(Tab tab, FragmentTransaction ft)
	{
		// When the given tab is selected, switch to the corresponding page in the ViewPager.
		viewPager.setCurrentItem(tab.getPosition());
	}

	@Override
	public void onTabReselected(Tab tab, FragmentTransaction ft)
	{
		// Do nothing.
	}

	@Override
	public void onTabUnselected(Tab tab, FragmentTransaction ft)
	{
		// Do nothing.
	}

	/**
	 * {@link FragmentPagerAdapter} subclass responsible for handling
	 * the individual {@link Fragment}s within the {@link ViewPager}.
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
			if (position == 0)
			{
				return new DolphinInfoFragment();
			}
			else if (position == 1)
			{
				return new CPUInfoFragment(); // CPU
			}
			else if (position == 2)    // GLES 2
			{
				return new GLES2InfoFragment();
			}
			else if (position == 3)    // GLES 3 or OpenGL (depending on circumstances)
			{
				if (eglHelper.supportsGLES3())
					return new GLES3InfoFragment();
				else
					return new GLInfoFragment(); // GLES3 not supported, but OpenGL is.
			}
			else if (position == 4)    // OpenGL fragment
			{
				return new GLInfoFragment();
			}

			// This should never happen.
			return null;
		}

		@Override
		public int getCount()
		{
			if (eglHelper.supportsGLES3() && eglHelper.supportsOpenGL())
			{
				return 5;
			}
			else if (!eglHelper.supportsGLES3() && !eglHelper.supportsOpenGL())
			{
				return 3;
			}
			else // Either regular OpenGL or GLES3 isn't supported
			{
				return 4;
			}
		}

		@Override
		public CharSequence getPageTitle(int position)
		{
			switch (position)
			{
				case 0:
					return getString(R.string.general);

				case 1:
					return getString(R.string.cpu);

				case 2:
					return getString(R.string.gles_two);

				case 3:
					return getString(R.string.gles_three);
				
				case 4:
					return getString(R.string.desktop_gl);

				default: // Should never happen
					return null;
			}
		}
	}
}
