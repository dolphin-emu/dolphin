package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.app.ListActivity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;
import net.simonvt.menudrawer.MenuDrawer;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class GameListView extends ListActivity {
	private GameListAdapter adapter;
	private MenuDrawer mDrawer;
	
    private SideMenuAdapter mAdapter;
    private static GameListView me;
    public static native String GetConfig(String Key, String Value, String Default);
    public static native void SetConfig(String Key, String Value, String Default);

    enum keyTypes {TYPE_STRING, TYPE_BOOL};
	
	private void Fill()
    {
		
		
		this.setTitle("Game List");
		List<GameListItem>fls = new ArrayList<GameListItem>();
		String Directories = GetConfig("General", "GCMPathes", "0");
		int intDirectories = Integer.parseInt(Directories);
		for (int a = 0; a < intDirectories; ++a)
		{
			String BrowseDir = GetConfig("General", "GCMPaths" + Integer.toString(a), "");
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
			            			fls.add(new GameListItem(getApplicationContext(), ff.getName(),"File Size: "+ff.length(),ff.getAbsolutePath()));
				}
			}
			catch(Exception ignored)
			{
			}
		}
		Collections.sort(fls);
		 
		adapter = new GameListAdapter(this,R.layout.main, fls);
		this.setListAdapter(adapter);
    }
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		super.onListItemClick(l, v, position, id);
		GameListItem o = adapter.getItem(position);
		if(!(o.getData().equalsIgnoreCase("folder")||o.getData().equalsIgnoreCase("parent directory")))
        {
			onFileClick(o.getPath());
		}
	}
	
	private void onFileClick(String o)
    {
    	Toast.makeText(this, "File Clicked: " + o, Toast.LENGTH_SHORT).show();
    	
        Intent intent = new Intent();
        intent.putExtra("Select", o);
        setResult(Activity.RESULT_OK, intent);
        
    	this.finish();
    }
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
                    String Directories = GetConfig("General", "GCMPathes", "0");
                    int intDirectories = Integer.parseInt(Directories);
                    Directories = Integer.toString(intDirectories + 1);
                    SetConfig("General", "GCMPathes", Directories);
                    SetConfig("General", "GCMPaths" + Integer.toString(intDirectories), FileName);

                    Fill();
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
                            SetConfig(Key, Value, strPref);
                        break;
                        case TYPE_BOOL:
                            boolean boolPref = prefs.getBoolean(Keys[a], true);
                            SetConfig(Key, Value, boolPref ? "True" : "False");
                        break;
                    }

                }
            break;
        }
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);
		me = this;
		 
		mDrawer = MenuDrawer.attach(this, MenuDrawer.MENU_DRAG_CONTENT);
		 
		
		Fill();
		 
		List<SideMenuItem>dir = new ArrayList<SideMenuItem>();
		dir.add(new SideMenuItem("Browse Folder", 0));
        dir.add(new SideMenuItem("Settings", 1));

        ListView mList = new ListView(this);
		mAdapter = new SideMenuAdapter(this,R.layout.sidemenu,dir);
		mList.setAdapter(mAdapter);
		mList.setOnItemClickListener(mItemClickListener);
		
		mDrawer.setMenuView(mList);
	}
    private AdapterView.OnItemClickListener mItemClickListener = new AdapterView.OnItemClickListener() {
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        	SideMenuItem o = mAdapter.getItem(position);
        	
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
        		default:
        		break;
        	}
            mDrawer.closeMenu();
        }
    };
    @Override
    public void setContentView(int layoutResID) {
        // This override is only needed when using MENU_DRAG_CONTENT.
    	mDrawer.setContentView(layoutResID);
        onContentChanged();
    }
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
            	mDrawer.toggleMenu();
                return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onBackPressed() {
        final int drawerState = mDrawer.getDrawerState();
        if (drawerState == MenuDrawer.STATE_OPEN || drawerState == MenuDrawer.STATE_OPENING) {
        	mDrawer.closeMenu();
            return;
        }

        super.onBackPressed();
    }
}
