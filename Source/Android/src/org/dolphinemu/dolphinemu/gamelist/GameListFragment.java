package org.dolphinemu.dolphinemu.gamelist;

import android.app.Activity;
import android.app.Fragment;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public final class GameListFragment extends Fragment
{
	private ListView mMainList;
	private GameListAdapter mGameAdapter;
	private static GameListActivity mMe;
	OnGameListZeroListener mCallback;

	public interface OnGameListZeroListener
	{
		public void onZeroFiles();
	}

	public GameListFragment()
	{
		// Empty constructor required for fragment subclasses
	}
	
	private void Fill()
	{
		List<GameListItem> fls = new ArrayList<GameListItem>();
		String Directories = NativeLibrary.GetConfig("Dolphin.ini", "General", "GCMPathes", "0");
		int intDirectories = Integer.parseInt(Directories);

		// Extensions to filter by.
		Set<String> exts = new HashSet<String>(Arrays.asList(".gcm", ".iso", ".wbfs", ".gcz", ".dol", ".elf", ".dff"));

		for (int a = 0; a < intDirectories; ++a)
		{
			String BrowseDir = NativeLibrary.GetConfig("Dolphin.ini", "General", "GCMPath" + a, "");
			File currentDir = new File(BrowseDir);
			File[] dirs = currentDir.listFiles();
			try
			{
				for(File entry : dirs)
				{
					String entryName = entry.getName();

					if (entryName.charAt(0) != '.')
					{
						if(!entry.isDirectory())
						{
							if (exts.contains(entryName.toLowerCase().substring(entryName.lastIndexOf('.'))))
								fls.add(new GameListItem(mMe.getApplicationContext(), entryName, getString(R.string.file_size)+entry.length(),entry.getAbsolutePath(), true));
						}
					}
				}
			}
			catch(Exception ignored)
			{
			}
		}
		Collections.sort(fls);
		
		// Remove any duplicate items from the list.
		// We don't need to index these in the game list more than once.
		//
		// This works by comparing the paths of items in the file list for equality,
		// so there should be no worries about accidentally removing a valid game.
		for (int i = 0; i < fls.size(); i++)
		{
		    int indexNext = i+1;
		    
		    if (indexNext < fls.size() && fls.get(indexNext).getPath().contains(fls.get(i).getPath()))
		    {
		        fls.remove(indexNext);
		    }
		}

		mGameAdapter = new GameListAdapter(mMe, R.layout.gamelist_layout, fls);
		mMainList.setAdapter(mGameAdapter);
		if (fls.size() == 0)
			mCallback.onZeroFiles();
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
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
			if(!(o.getData().equalsIgnoreCase(getString(R.string.folder))||o.getData().equalsIgnoreCase(getString(R.string.parent_directory))))
			{
				onFileClick(o.getPath());
			}
		}
	};
	
	private void onFileClick(String o)
	{
		Toast.makeText(mMe, getString(R.string.file_clicked) + o, Toast.LENGTH_SHORT).show();

		Intent intent = new Intent();
		intent.putExtra("Select", o);
		mMe.setResult(Activity.RESULT_OK, intent);
		mMe.finish();
	}
	
	@Override
	public void onAttach(Activity activity)
	{
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try
		{
			mCallback = (OnGameListZeroListener) activity;
			mMe = (GameListActivity) activity;
		}
		catch (ClassCastException e)
		{
			throw new ClassCastException(activity.toString()
					+ " must implement OnGameListZeroListener");
		}
	}
}