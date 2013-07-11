package org.dolphinemu.dolphinemu;

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
import java.util.Collections;
import java.util.List;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class GameListFragment extends Fragment
{
	private ListView mMainList;
	private GameListAdapter mGameAdapter;
	private static GameListActivity mMe;
	OnGameListZeroListener mCallback;

	public interface OnGameListZeroListener {
		public void onZeroFiles();
	}

	public GameListFragment() {
		// Empty constructor required for fragment subclasses
	}
	private void Fill()
	{
		List<GameListItem> fls = new ArrayList<GameListItem>();
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
								fls.add(new GameListItem(mMe.getApplicationContext(), ff.getName(),"File Size: "+ff.length(),ff.getAbsolutePath(), true));
				}
			}
			catch(Exception ignored)
			{
			}
		}
		Collections.sort(fls);

		mGameAdapter = new GameListAdapter(mMe, R.layout.gamelist_layout, fls);
		mMainList.setAdapter(mGameAdapter);
		if (fls.size() == 0)
			mCallback.onZeroFiles();
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
		Toast.makeText(mMe, "File Clicked: " + o, Toast.LENGTH_SHORT).show();

		Intent intent = new Intent();
		intent.putExtra("Select", o);
		mMe.setResult(Activity.RESULT_OK, intent);
		mMe.finish();
	}
	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			mCallback = (OnGameListZeroListener) activity;
			mMe = (GameListActivity) activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement OnGameListZeroListener");
		}
	}
}