package org.dolphinemu.dolphinemu.ui.main;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.TabLayout;
import android.support.v4.view.ViewPager;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.AddDirectoryActivity;
import org.dolphinemu.dolphinemu.adapters.PlatformPagerAdapter;
import org.dolphinemu.dolphinemu.model.GameProvider;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesView;
import org.dolphinemu.dolphinemu.ui.settings.SettingsActivity;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity implements MainView
{
	private ViewPager mViewPager;
	private Toolbar mToolbar;
	private TabLayout mTabLayout;
	private FloatingActionButton mFab;

	private MainPresenter mPresenter = new MainPresenter(this);

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		findViews();

		setSupportActionBar(mToolbar);

		mTabLayout.setupWithViewPager(mViewPager);

		// Set up the FAB.
		mFab.setOnClickListener(new View.OnClickListener()
		{
			@Override
			public void onClick(View view)
			{
				mPresenter.onFabClick();
			}
		});

		mPresenter.onCreate();

		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		// TODO Split some of this stuff into Application.onCreate()
		if (savedInstanceState == null)
			StartupHandler.HandleInit(this);

		if (PermissionsHandler.hasWriteAccess(this))
		{
			PlatformPagerAdapter platformPagerAdapter = new PlatformPagerAdapter(
					getSupportFragmentManager(), this);
			mViewPager.setAdapter(platformPagerAdapter);
		} else {
			mViewPager.setVisibility(View.INVISIBLE);
		}
	}

	// TODO: Replace with a ButterKnife injection.
	private void findViews()
	{
		mToolbar = (Toolbar) findViewById(R.id.toolbar_main);
		mViewPager = (ViewPager) findViewById(R.id.pager_platforms);
		mTabLayout = (TabLayout) findViewById(R.id.tabs_platforms);
		mFab = (FloatingActionButton) findViewById(R.id.button_add_directory);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.menu_game_grid, menu);
		return true;
	}

	/**
	 * MainView
	 */

	@Override
	public void setVersionString(String version)
	{
		mToolbar.setSubtitle(version);
	}

	@Override
	public void refresh()
	{
		getContentResolver().insert(GameProvider.URI_REFRESH, null);
		refreshFragment();
	}

	@Override
	public void refreshFragmentScreenshot(int fragmentPosition)
	{
		// Invalidate Picasso image so that the new screenshot is animated in.
		Platform platform = Platform.fromPosition(mViewPager.getCurrentItem());
		PlatformGamesView fragment = getPlatformGamesView(platform);

		if (fragment != null)
		{
			fragment.refreshScreenshotAtPosition(fragmentPosition);
		}
	}

	@Override
	public void launchSettingsActivity(String menuTag)
	{
		SettingsActivity.launch(this, menuTag);
	}

	@Override
	public void launchFileListActivity()
	{
		AddDirectoryActivity.launch(this);
	}

	@Override
	public void showGames(Platform platform, Cursor games)
	{
		// no-op. Handled by PlatformGamesFragment.
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
		mPresenter.handleActivityResult(requestCode, resultCode);
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
		switch (requestCode) {
			case PermissionsHandler.REQUEST_CODE_WRITE_PERMISSION:
				if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
					StartupHandler.copyAssetsIfNeeded(this);

					PlatformPagerAdapter platformPagerAdapter = new PlatformPagerAdapter(
							getSupportFragmentManager(), this);
					mViewPager.setAdapter(platformPagerAdapter);
					mTabLayout.setupWithViewPager(mViewPager);
					mViewPager.setVisibility(View.VISIBLE);
				} else {
					Toast.makeText(this, R.string.write_permission_needed, Toast.LENGTH_SHORT)
							.show();
				}
				break;
			default:
				super.onRequestPermissionsResult(requestCode, permissions, grantResults);
				break;
		}
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
		return mPresenter.handleOptionSelection(item.getItemId());
	}

	private void refreshFragment()
	{

		Platform platform = Platform.fromPosition(mViewPager.getCurrentItem());
		PlatformGamesView fragment = getPlatformGamesView(platform);
		if (fragment != null)
		{
			fragment.refresh();
		}
	}

	@Nullable
	private PlatformGamesView getPlatformGamesView(Platform platform)
	{
		String fragmentTag = "android:switcher:" + mViewPager.getId() + ":" + platform;

		return (PlatformGamesView) getSupportFragmentManager().findFragmentByTag(fragmentTag);
	}
}
