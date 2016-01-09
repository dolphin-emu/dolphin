package org.dolphinemu.dolphinemu.activities;

import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.Nullable;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.TabLayout;
import android.support.v4.view.ViewPager;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.PlatformPagerAdapter;
import org.dolphinemu.dolphinemu.fragments.PlatformGamesFragment;
import org.dolphinemu.dolphinemu.model.Game;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.model.GameProvider;
import org.dolphinemu.dolphinemu.services.AssetCopyService;
import org.dolphinemu.dolphinemu.utils.StartupHandler;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity
{
	public static final int REQUEST_ADD_DIRECTORY = 1;
	public static final int REQUEST_EMULATE_GAME = 2;

	private ViewPager mViewPager;
	private PlatformPagerAdapter mPlatformPagerAdapter;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		// Set up the Toolbar.
		Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar_main);
		setSupportActionBar(toolbar);

		// TODO Rather than calling into native code, this should use the commented line below.
		// String versionName = BuildConfig.VERSION_NAME;
		String versionName = NativeLibrary.GetVersionString();
		toolbar.setSubtitle(versionName);

		// Set up the Tab bar.
		mViewPager = (ViewPager) findViewById(R.id.pager_platforms);

		mPlatformPagerAdapter = new PlatformPagerAdapter(getFragmentManager(), this);
		mViewPager.setAdapter(mPlatformPagerAdapter);

		TabLayout tabLayout = (TabLayout) findViewById(R.id.tabs_platforms);
		tabLayout.setupWithViewPager(mViewPager);

		// Set up the FAB.
		FloatingActionButton buttonAddDirectory = (FloatingActionButton) findViewById(R.id.button_add_directory);
		buttonAddDirectory.setOnClickListener(new View.OnClickListener()
		{
			@Override
			public void onClick(View view)
			{
				Intent fileChooser = new Intent(MainActivity.this, AddDirectoryActivity.class);

				// The second argument to this method is read below in onActivityResult().
				startActivityForResult(fileChooser, REQUEST_ADD_DIRECTORY);
			}
		});

		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		if (savedInstanceState == null)
			StartupHandler.HandleInit(this);
	}

	/**
	 * Callback from AddDirectoryActivity. Applies any changes necessary to the GameGridActivity.
	 *
	 * @param requestCode An int describing whether the Activity that is returning did so successfully.
	 * @param resultCode  An int describing what Activity is giving us this callback.
	 * @param result      The information the returning Activity is providing us.
	 */
	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent result)
	{
		switch (requestCode)
		{
			case REQUEST_ADD_DIRECTORY:
				// If the user picked a file, as opposed to just backing out.
				if (resultCode == RESULT_OK)
				{
					// Sanity check to make sure the Activity that just returned was the AddDirectoryActivity;
					// other activities might use this callback in the future (don't forget to change Javadoc!)
					if (requestCode == REQUEST_ADD_DIRECTORY)
					{
						refreshFragment();
					}
				}
				break;

			case REQUEST_EMULATE_GAME:
				// Invalidate Picasso image so that the new screenshot is animated in.
				PlatformGamesFragment fragment = getPlatformFragment(mViewPager.getCurrentItem());

				if (fragment != null)
				{
					fragment.refreshScreenshotAtPosition(resultCode);
				}
				break;
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.menu_game_grid, menu);
		return true;
	}

	/**
	 * Called by the framework whenever any actionbar/toolbar icon is clicked.
	 *
	 * @param item The icon that was clicked on.
	 * @return True if the event was handled, false to bubble it up to the OS.
	 */
	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		switch (item.getItemId())
		{
			case R.id.menu_settings:
				// Launch the Settings Actvity.
				Intent settings = new Intent(this, SettingsActivity.class);
				startActivity(settings);
				return true;

			case R.id.menu_refresh:
				getContentResolver().insert(GameProvider.URI_REFRESH, null);
				refreshFragment();

				return true;
		}

		return false;
	}

	public void refreshFragment()
	{
		PlatformGamesFragment fragment = getPlatformFragment(mViewPager.getCurrentItem());
		if (fragment != null)
		{
			fragment.refresh();
		}
	}

	@Nullable
	public PlatformGamesFragment getPlatformFragment(int platform)
	{
		String fragmentTag = "android:switcher:" + mViewPager.getId() + ":" + platform;

		return (PlatformGamesFragment) getFragmentManager().findFragmentByTag(fragmentTag);
	}
}
