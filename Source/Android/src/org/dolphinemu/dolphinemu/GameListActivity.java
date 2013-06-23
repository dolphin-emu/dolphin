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
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class GameListActivity extends Activity {

	enum keyTypes {TYPE_STRING, TYPE_BOOL};

	private ActionBarDrawerToggle mDrawerToggle;
	private DrawerLayout mDrawerLayout;
	private SideMenuAdapter mDrawerAdapter;
	private ListView mDrawerList;

	private static GameListActivity me;

	public static class GameListFragment extends Fragment {
		private ListView mMainList;
		private GameListAdapter mGameAdapter;

		public GameListFragment() {
			// Empty constructor required for fragment subclasses
		}
		private void Fill()
		{
			List<GameListItem>fls = new ArrayList<GameListItem>();
			String Directories = NativeLibrary.GetConfig("Dolphin.ini", "General", "GCMPathes", "0");
			int intDirectories = Integer.parseInt(Directories);
			for (int a = 0; a < intDirectories; ++a)
			{
				String BrowseDir = NativeLibrary.GetConfig("Dolphin.ini", "General", "GCMPath" + Integer.toString(a), "");
				File currentDir = new File(BrowseDir);
				File[]dirs = currentDir.listFiles();
				try
				{
					for(File ff: dirs)
					{
						if (ff.getName().charAt(0) != '.')
							if(!ff.isDirectory())
								if (ff.getName().toLowerCase().contains(".gcm") ||
										ff.getName().toLowerCase().contains(".iso") ||
										ff.getName().toLowerCase().contains(".wbfs") ||
										ff.getName().toLowerCase().contains(".gcz") ||
										ff.getName().toLowerCase().contains(".dol") ||
										ff.getName().toLowerCase().contains(".elf") ||
										ff.getName().toLowerCase().contains(".dff"))
									fls.add(new GameListItem(me.getApplicationContext(), ff.getName(),"File Size: "+ff.length(),ff.getAbsolutePath()));
					}
				}
				catch(Exception ignored)
				{
				}
			}
			Collections.sort(fls);

			mGameAdapter = new GameListAdapter(me, R.layout.gamelist_layout, fls);
			mMainList.setAdapter(mGameAdapter);
		}

		@Override
		public View onCreateView(LayoutInflater inflater, ViewGroup container,
		                         Bundle savedInstanceState) {
			View rootView = inflater.inflate(R.layout.gamelist_listview, container, false);
			mMainList = (ListView) rootView.findViewById(R.id.gamelist);
			mMainList.setOnItemClickListener(mGameItemClickListener);

			Fill();

			return mMainList;
		}
		private AdapterView.OnItemClickListener mGameItemClickListener = new AdapterView.OnItemClickListener()
		{
			public void onItemClick(AdapterView<?> parent, View view, int position, long id)
			{
				GameListItem o = mGameAdapter.getItem(position);
				if(!(o.getData().equalsIgnoreCase("folder")||o.getData().equalsIgnoreCase("parent directory")))
				{
					onFileClick(o.getPath());
				}
			}
		};
		private void onFileClick(String o)
		{
			Toast.makeText(me, "File Clicked: " + o, Toast.LENGTH_SHORT).show();

			Intent intent = new Intent();
			intent.putExtra("Select", o);
			me.setResult(Activity.RESULT_OK, intent);

			me.finish();
		}
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.gamelist_activity);
		me = this;

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
					Toast.makeText(me, "Loading up the browser", Toast.LENGTH_SHORT).show();
					Intent ListIntent = new Intent(me, FolderBrowser.class);
					startActivityForResult(ListIntent, 1);
					break;
				case 1:
					Toast.makeText(me, "Loading up settings", Toast.LENGTH_SHORT).show();
					Intent SettingIntent = new Intent(me, PrefsActivity.class);
					startActivityForResult(SettingIntent, 2);
					break;
				case 2:
					Toast.makeText(me, "Loading up gamepad config", Toast.LENGTH_SHORT).show();
					Intent ConfigIntent = new Intent(me, InputConfigActivity.class);
					startActivityForResult(ConfigIntent, 3);
					break;
				case 3:
					Toast.makeText(me, "Loading up About", Toast.LENGTH_SHORT).show();
					Intent AboutIntent = new Intent(me, AboutActivity.class);
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
