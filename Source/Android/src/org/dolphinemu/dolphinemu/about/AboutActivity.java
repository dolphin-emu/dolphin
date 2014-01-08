package org.dolphinemu.dolphinemu.about;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.settings.video.VideoSettingsFragment;

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
	private boolean supportsGles3;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Set the view pager
		setContentView(R.layout.viewpager);
		viewPager = (ViewPager) findViewById(R.id.pager);

		// Check if GLES3 is supported
		supportsGles3 = VideoSettingsFragment.SupportsGLES3();

		// Initialize the ViewPager adapter.
		final ViewPagerAdapter adapter = new ViewPagerAdapter(getFragmentManager());
		viewPager.setAdapter(adapter);

		// Set up the ActionBar
		final ActionBar actionBar = getActionBar();
		actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
		actionBar.addTab(actionBar.newTab().setText(R.string.general).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.cpu).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.gles_two).setTabListener(this));
		if (supportsGles3)
			actionBar.addTab(actionBar.newTab().setText(R.string.gles_three).setTabListener(this));
		// TODO: Check if Desktop GL is possible before enabling.
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
			switch (position)
			{
				case 0: return new DolphinInfoFragment();
				// TODO: The rest of these fragments
				case 1: return new Fragment(); // CPU
				case 2: return new Fragment(); // GLES 2
				case 3: return new Fragment(); // GLES 3
				case 4: return new Fragment(); // Desktop GL

				default: // Should never happen
					return null;
			}
		}

		@Override
		public int getCount()
		{
			// TODO: In the future, make sure to take into account
			//       whether or not regular Desktop GL is possible.
			if (supportsGles3)
			{
				return 5;
			}
			else
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
