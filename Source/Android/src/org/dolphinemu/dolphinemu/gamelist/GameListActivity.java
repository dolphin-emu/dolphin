/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.gamelist;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Fragment;
import android.app.FragmentTransaction;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.ActionBarDrawerToggle;
import android.support.v4.widget.DrawerLayout;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

import org.dolphinemu.dolphinemu.AssetCopyService;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.about.AboutActivity;
import org.dolphinemu.dolphinemu.folderbrowser.FolderBrowser;
import org.dolphinemu.dolphinemu.settings.PrefsActivity;
import org.dolphinemu.dolphinemu.sidemenu.SideMenuAdapter;
import org.dolphinemu.dolphinemu.sidemenu.SideMenuItem;

import java.util.ArrayList;
import java.util.List;

/**
 * The activity that implements all of the functions
 * for the game list.
 */
public final class GameListActivity extends Activity
		implements GameListFragment.OnGameListZeroListener
{
	private int mCurFragmentNum = 0;

	private ActionBarDrawerToggle mDrawerToggle;
	private DrawerLayout mDrawerLayout;
	private SideMenuAdapter mDrawerAdapter;
	private ListView mDrawerList;

	/**
	 * Called from the {@link GameListFragment}.
	 * <p>
	 * This is called when there are no games
	 * currently present within the game list.
	 */
	public void onZeroFiles()
	{
		mDrawerLayout.openDrawer(mDrawerList);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.gamelist_activity);

		mDrawerLayout = (DrawerLayout) findViewById(R.id.drawer_layout);
		mDrawerList = (ListView) findViewById(R.id.left_drawer);

		// Construct list of items to add to the side menu.
		List<SideMenuItem> dir = new ArrayList<SideMenuItem>();
		dir.add(new SideMenuItem(getString(R.string.game_list), 0));
		dir.add(new SideMenuItem(getString(R.string.browse_folder), 1));
		dir.add(new SideMenuItem(getString(R.string.settings), 2));
		dir.add(new SideMenuItem(getString(R.string.about), 3));

		mDrawerAdapter = new SideMenuAdapter(this, R.layout.sidemenu, dir);
		mDrawerList.setAdapter(mDrawerAdapter);
		mDrawerList.setOnItemClickListener(mMenuItemClickListener);

		// Enable ActionBar app icon to behave as action to toggle nav drawer
		getActionBar().setDisplayHomeAsUpEnabled(true);
		getActionBar().setHomeButtonEnabled(true);

		// ActionBarDrawerToggle ties together the the proper interactions
		// between the sliding drawer and the action bar app icon
		mDrawerToggle = new ActionBarDrawerToggle(
				this,                  /* Host Activity */
				mDrawerLayout,         /* DrawerLayout object */
				R.drawable.ic_drawer,  /* Navigation drawer image to replace 'Up' caret */
				R.string.drawer_open,  /* "open drawer" description for accessibility */
				R.string.drawer_close  /* "close drawer" description for accessibility */
		) {
			public void onDrawerClosed(View view) {
				invalidateOptionsMenu(); // creates call to onPrepareOptionsMenu()
			}

			public void onDrawerOpened(View drawerView) {
				invalidateOptionsMenu(); // creates call to onPrepareOptionsMenu()
			}
		};
		mDrawerLayout.setDrawerListener(mDrawerToggle);


		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		if (savedInstanceState == null)
		{
			// Copy assets into appropriate locations.
			Intent copyAssets = new Intent(this, AssetCopyService.class);
			startService(copyAssets);

			// Display the game list fragment.
			final GameListFragment gameList = new GameListFragment();
			FragmentTransaction ft = getFragmentManager().beginTransaction();
			ft.replace(R.id.content_frame, gameList);
			ft.commit();
		}
		

		// Create an alert telling them that their phone sucks
		if (Build.CPU_ABI.contains("arm") && !NativeLibrary.SupportsNEON())
		{
			AlertDialog.Builder builder = new AlertDialog.Builder(this);
			builder.setTitle(R.string.device_compat_warning);
			builder.setMessage(R.string.device_compat_warning_msg);
			builder.setPositiveButton(R.string.yes, null);
			builder.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which)
				{
					finish();
				}
			});
			builder.show();
		}
	}

	/**
	 * Switches to the {@link Fragment} represented
	 * by the given ID number.
	 * 
	 * @param toPage the number representing the {@link Fragment} to switch to.
	 */
	public void SwitchPage(int toPage)
	{
		if (mCurFragmentNum == toPage)
			return;

		switch(toPage)
		{
			case 0: // Game list
			{
				// We use the title section as the browser directory tracker in the folder browser.
				// Make sure we flip the title back if we're coming from that fragment.
				if (mCurFragmentNum == 1)
					setTitle(R.string.app_name);

				mCurFragmentNum = 0;
				final GameListFragment gameList = new GameListFragment();
				FragmentTransaction ft = getFragmentManager().beginTransaction();
				ft.replace(R.id.content_frame, gameList);
				ft.commit();
				invalidateOptionsMenu();
			}
			break;

			case 1: // Folder Browser
			{
				mCurFragmentNum = 1;
				final FolderBrowser folderBrowser = new FolderBrowser();
				FragmentTransaction ft = getFragmentManager().beginTransaction();
				ft.replace(R.id.content_frame, folderBrowser);
				ft.addToBackStack(null);
				ft.commit();
				invalidateOptionsMenu();
			}
			break;

			case 2: // Settings
			{
				Intent intent = new Intent(this, PrefsActivity.class);
				startActivity(intent);
			}
			break;

			case 3: // About
			{
				Intent intent = new Intent(this, AboutActivity.class);
				startActivity(intent);
			}
			break;

			default:
				break;
		}
	}

	private final AdapterView.OnItemClickListener mMenuItemClickListener = new AdapterView.OnItemClickListener()
	{
		public void onItemClick(AdapterView<?> parent, View view, int position, long id)
		{
			SideMenuItem o = mDrawerAdapter.getItem(position);
			mDrawerLayout.closeDrawer(mDrawerList);
			SwitchPage(o.getID());
		}
	};

	/**
	 * When using the ActionBarDrawerToggle, you must call it during
	 * onPostCreate() and onConfigurationChanged()...
	 */
	@Override
	protected void onPostCreate(Bundle savedInstanceState)
	{
		super.onPostCreate(savedInstanceState);

		// Sync the toggle state after onRestoreInstanceState has occurred.
		mDrawerToggle.syncState();
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig)
	{
		super.onConfigurationChanged(newConfig);

		// Pass any configuration change to the drawer toggle
		mDrawerToggle.onConfigurationChanged(newConfig);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		// Only show this in the game list.
		if (mCurFragmentNum == 0)
		{
			MenuInflater inflater = getMenuInflater();
			inflater.inflate(R.menu.gamelist_menu, menu);
			return true;
		}

		return false;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		// The action bar home/up action should open or close the drawer.
		// ActionBarDrawerToggle will take care of this.
		if (mDrawerToggle.onOptionsItemSelected(item))
		{
			return true;
		}

		// If clear game list is pressed.
		if (item.getItemId() == R.id.clearGameList)
		{
			AlertDialog.Builder builder = new AlertDialog.Builder(this);
			builder.setTitle(R.string.clear_game_list);
			builder.setMessage(getString(R.string.clear_game_list_confirm));
			builder.setNegativeButton(R.string.no, null);
			builder.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which)
				{
					String directories = NativeLibrary.GetConfig("Dolphin.ini", "General", "ISOPaths", "0");
					int intDirs = Integer.parseInt(directories);

					for (int i = 0; i < intDirs; i++)
					{
						NativeLibrary.SetConfig("Dolphin.ini", "General", "ISOPath" + i, "");
					}

					// Since we flushed all paths, we signify this in the ini.
					NativeLibrary.SetConfig("Dolphin.ini", "General", "ISOPaths", "0");

					// Now finally, clear the game list.
					((GameListFragment) getFragmentManager().findFragmentById(R.id.content_frame)).clearGameList();
				}
			});

			builder.show();
		}

		return super.onOptionsItemSelected(item);
	}

	@Override
	public void onSaveInstanceState(Bundle outState)
	{
		super.onSaveInstanceState(outState);

		outState.putInt("currentFragmentNum", mCurFragmentNum);
	}

	@Override
	public void onRestoreInstanceState(Bundle savedInstanceState)
	{
		super.onRestoreInstanceState(savedInstanceState);

		mCurFragmentNum = savedInstanceState.getInt("currentFragmentNum");
	}

	@Override
	public void onBackPressed()
	{
		if (mCurFragmentNum == 0)
		{
			finish();
		}
		else
		{
			SwitchPage(0);
		}
	}
}
