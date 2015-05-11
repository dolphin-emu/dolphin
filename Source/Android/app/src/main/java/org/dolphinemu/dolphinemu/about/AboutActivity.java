/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.os.Bundle;
import android.support.v13.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;


/**
 * Activity for the about menu, which displays info
 * related to the CPU and GPU. Along with misc other info.
 */
public final class AboutActivity extends Activity
{
	private final EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Set the view pager
		setContentView(R.layout.viewpager);

		// Initialize the viewpager.
		final ViewPager viewPager = (ViewPager) findViewById(R.id.pager);
		viewPager.setAdapter(new ViewPagerAdapter(getFragmentManager()));
	}

	@Override
	public void onDestroy()
	{
		super.onDestroy();

		eglHelper.closeHelper();
	}

	/**
	 * {@link FragmentPagerAdapter} subclass responsible for handling
	 * the individual {@link Fragment}s within the {@link ViewPager}.
	 */
	private final class ViewPagerAdapter extends FragmentPagerAdapter
	{
		private final String[] pageTitles = {
			getString(R.string.general),
			getString(R.string.cpu),
			getString(R.string.gles_two),
			getString(R.string.gles_three),
			getString(R.string.desktop_gl),
		};

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
			return pageTitles[position];
		}
	}
}
