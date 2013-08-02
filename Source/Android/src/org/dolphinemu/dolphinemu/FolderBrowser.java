package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.app.Fragment;
import android.os.Bundle;
import android.os.Environment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import java.io.File;
import java.util.*;

public class FolderBrowser extends Fragment {
	private Activity m_activity;
	private FolderBrowserAdapter adapter;
	private ListView mDrawerList;
	private View rootView;
	private static File currentDir = null;

	// Populates the FolderView with the given currDir's contents.
	private void Fill(File currDir)
    {
	    m_activity.setTitle("Current Dir: " + currDir.getName());
		File[] dirs = currDir.listFiles();
		List<GameListItem>dir = new ArrayList<GameListItem>();
		List<GameListItem>fls = new ArrayList<GameListItem>();

		// Supported extensions to filter by
		Set<String> validExts = new HashSet<String>(Arrays.asList(".gcm", ".iso", ".wbfs", ".gcz", ".dol", ".elf", ".dff"));
		Set<String> archiveExts = new HashSet<String>(Arrays.asList(".zip", ".rar", ".7z"));

		// Search for any directories or supported files within the current dir.
		try
		{
			for(File entry : dirs)
			{
				String entryName = entry.getName();

				if (entryName.charAt(0) != '.')
				{
					if(entry.isDirectory())
					{
						dir.add(new GameListItem(m_activity, entryName,"Folder",entry.getAbsolutePath(), true));
					}
					else
					{
						if (validExts.contains(entryName.toLowerCase().substring(entryName.lastIndexOf('.'))))
						{
							fls.add(new GameListItem(m_activity, entryName,"File Size: "+entry.length(),entry.getAbsolutePath(), true));
						}
						else if (archiveExts.contains(entryName.toLowerCase().substring(entryName.lastIndexOf('.'))))
						{
							fls.add(new GameListItem(m_activity, entryName,"File Size: "+entry.length(),entry.getAbsolutePath(), false));
						}
					}
				}
			}
		}
		catch(Exception ignored)
		{
		}

		Collections.sort(dir);
		Collections.sort(fls);
		dir.addAll(fls);

		// Check for a parent directory to the one we're currently in.
		if (!currDir.getPath().equalsIgnoreCase("/"))
			dir.add(0, new GameListItem(m_activity, "..", "Parent Directory", currDir.getParent(), true));

		adapter = new FolderBrowserAdapter(m_activity, R.layout.folderbrowser, dir);
	    mDrawerList = (ListView) rootView.findViewById(R.id.gamelist);
	    mDrawerList.setAdapter(adapter);
	    mDrawerList.setOnItemClickListener(mMenuItemClickListener);
    }
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
	                         Bundle savedInstanceState)
	{
		if(currentDir == null)
			currentDir = new File(Environment.getExternalStorageDirectory().getPath());

		rootView = inflater.inflate(R.layout.gamelist_listview, container, false);

		Fill(currentDir);
		return mDrawerList;
	}

	private AdapterView.OnItemClickListener mMenuItemClickListener = new AdapterView.OnItemClickListener()
	{
		public void onItemClick(AdapterView<?> parent, View view, int position, long id)
		{
			GameListItem o = adapter.getItem(position);
			if(o.getData().equalsIgnoreCase("folder") || o.getData().equalsIgnoreCase("parent directory"))
			{
				currentDir = new File(o.getPath());
				Fill(currentDir);
			}
			else
				if (o.isValid())
					FolderSelected();
				else
					Toast.makeText(m_activity, "Can not use compressed file types.", Toast.LENGTH_LONG).show();
		}
	};

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			m_activity = activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement OnGameListZeroListener");
		}
	}
	private void FolderSelected()
	{
		String Directories = NativeLibrary.GetConfig("Dolphin.ini", "General", "GCMPathes", "0");
		int intDirectories = Integer.parseInt(Directories);
		Directories = Integer.toString(intDirectories + 1);
		NativeLibrary.SetConfig("Dolphin.ini", "General", "GCMPathes", Directories);
		NativeLibrary.SetConfig("Dolphin.ini", "General", "GCMPath" + Integer.toString(intDirectories), currentDir.getPath());

		((GameListActivity)m_activity).SwitchPage(0);
	}
}
