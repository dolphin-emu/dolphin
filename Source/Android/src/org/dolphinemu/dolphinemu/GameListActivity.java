package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.ActionBarDrawerToggle;
import android.support.v4.widget.DrawerLayout;
import android.view.Menu;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class GameListActivity extends Activity
		implements GameListFragment.OnGameListZeroListener{

	enum keyTypes {TYPE_STRING, TYPE_BOOL};

	private ActionBarDrawerToggle mDrawerToggle;
	private DrawerLayout mDrawerLayout;
	private SideMenuAdapter mDrawerAdapter;
	private ListView mDrawerList;

	private static GameListActivity mMe;

	// Called from the game list fragment
	public void onZeroFiles()
	{
		mDrawerLayout.openDrawer(mDrawerList);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.gamelist_activity);
		mMe = this;

		mDrawerLayout = (DrawerLayout) findViewById(R.id.drawer_layout);
		mDrawerList = (ListView) findViewById(R.id.left_drawer);

		List<SideMenuItem> dir = new ArrayList<SideMenuItem>();
		dir.add(new SideMenuItem("Browse Folder", 0));
		dir.add(new SideMenuItem("Settings", 1));
		dir.add(new SideMenuItem("Gamepad Config", 2));
		dir.add(new SideMenuItem("About", 3));

		mDrawerAdapter = new SideMenuAdapter(this, R.layout.sidemenu, dir);
		mDrawerList.setAdapter(mDrawerAdapter);
		mDrawerList.setOnItemClickListener(mMenuItemClickListener);

		// enable ActionBar app icon to behave as action to toggle nav drawer
		getActionBar().setDisplayHomeAsUpEnabled(true);
		getActionBar().setHomeButtonEnabled(true);

		// ActionBarDrawerToggle ties together the the proper interactions
		// between the sliding drawer and the action bar app icon
		mDrawerToggle = new ActionBarDrawerToggle(
				this,                  /* host Activity */
				mDrawerLayout,         /* DrawerLayout object */
				R.drawable.ic_drawer,  /* nav drawer image to replace 'Up' caret */
				R.string.drawer_open,  /* "open drawer" description for accessibility */
				R.string.drawer_close  /* "close drawer" description for accessibility */
		) {
		};
		mDrawerLayout.setDrawerListener(mDrawerToggle);

		recreateFragment();
	}

	private void recreateFragment()
	{
		Fragment fragment = new GameListFragment();
		FragmentManager fragmentManager = getFragmentManager();
		fragmentManager.beginTransaction().replace(R.id.content_frame, fragment).commit();
	}

	private AdapterView.OnItemClickListener mMenuItemClickListener = new AdapterView.OnItemClickListener()
	{
		public void onItemClick(AdapterView<?> parent, View view, int position, long id)
		{
			SideMenuItem o = mDrawerAdapter.getItem(position);
			mDrawerLayout.closeDrawer(mDrawerList);

			switch(o.getID())
			{
				case 0:
					Toast.makeText(mMe, "Loading up the browser", Toast.LENGTH_SHORT).show();
					Intent ListIntent = new Intent(mMe, FolderBrowser.class);
					startActivityForResult(ListIntent, 1);
					break;
				case 1:
					Toast.makeText(mMe, "Loading up settings", Toast.LENGTH_SHORT).show();
					Intent SettingIntent = new Intent(mMe, PrefsActivity.class);
					startActivityForResult(SettingIntent, 2);
					break;
				case 2:
					Toast.makeText(mMe, "Loading up gamepad config", Toast.LENGTH_SHORT).show();
					Intent ConfigIntent = new Intent(mMe, InputConfigActivity.class);
					startActivityForResult(ConfigIntent, 3);
					break;
				case 3:
					Toast.makeText(mMe, "Loading up About", Toast.LENGTH_SHORT).show();
					Intent AboutIntent = new Intent(mMe, AboutActivity.class);
					startActivityForResult(AboutIntent, 3);
					break;
				default:
					break;
			}

		}
	};
	@Override
	public void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		super.onActivityResult(requestCode, resultCode, data);

		switch (requestCode)
		{
			// Browse
			case 1:
				if (resultCode == Activity.RESULT_OK)
				{
					String FileName = data.getStringExtra("Select");
					Toast.makeText(this, "Folder Selected: " + FileName, Toast.LENGTH_SHORT).show();
					String Directories = NativeLibrary.GetConfig("Dolphin.ini", "General", "GCMPathes", "0");
					int intDirectories = Integer.parseInt(Directories);
					Directories = Integer.toString(intDirectories + 1);
					NativeLibrary.SetConfig("Dolphin.ini", "General", "GCMPathes", Directories);
					NativeLibrary.SetConfig("Dolphin.ini", "General", "GCMPath" + Integer.toString(intDirectories), FileName);

					recreateFragment();
				}
				break;
			// Settings
			case 2:

				String Keys[] = {
						"cpupref",
						"dualcorepref",
						"gpupref",
				};
				String ConfigKeys[] = {
						"Core-CPUCore",
						"Core-CPUThread",
						"Core-GFXBackend",
				};

				keyTypes KeysTypes[] = {
						keyTypes.TYPE_STRING,
						keyTypes.TYPE_BOOL,
						keyTypes.TYPE_STRING,
				};
				SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());

				// Set our preferences here
				for (int a = 0; a < Keys.length; ++a)
				{
					String ConfigValues[] = ConfigKeys[a].split("-");
					String Key = ConfigValues[0];
					String Value = ConfigValues[1];

					switch(KeysTypes[a])
					{
						case TYPE_STRING:
							String strPref = prefs.getString(Keys[a], "");
							NativeLibrary.SetConfig("Dolphin.ini", Key, Value, strPref);
							break;
						case TYPE_BOOL:
							boolean boolPref = prefs.getBoolean(Keys[a], true);
							NativeLibrary.SetConfig("Dolphin.ini", Key, Value, boolPref ? "True" : "False");
							break;
					}

				}
				break;
			case 3: // Gamepad settings
		        /* Do Nothing */
				break;
		}
	}
	/**
	 * When using the ActionBarDrawerToggle, you must call it during
	 * onPostCreate() and onConfigurationChanged()...
	 */

	@Override
	protected void onPostCreate(Bundle savedInstanceState) {
		super.onPostCreate(savedInstanceState);
		// Sync the toggle state after onRestoreInstanceState has occurred.
		mDrawerToggle.syncState();
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		// Pass any configuration change to the drawer toggls
		mDrawerToggle.onConfigurationChanged(newConfig);
	}

	/* Called whenever we call invalidateOptionsMenu() */
	@Override
	public boolean onPrepareOptionsMenu(Menu menu) {
		return super.onPrepareOptionsMenu(menu);
	}
	public void onBackPressed()
	{

	}


}
