/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.gamelist;

import android.app.Activity;
import android.app.ListFragment;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import android.widget.Toast;
import android.app.Service;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.emulation.EmulationActivity;
import org.dolphinemu.dolphinemu.emulation.GameListActivity;


/**
 * The {@link ListFragment} responsible for displaying the game list.
 */
public final class GameListFragment extends ListFragment
{
	private GameListAdapter mGameAdapter;
	private OnGameListZeroListener mCallback;

	/**
	 * Interface that defines how to handle the case
	 * when there are zero games in the game list.
	 */
	public interface OnGameListZeroListener
	{
		/**
		 * This is called when there are no games
		 * currently present within the game list.
		 */
		void onZeroFiles();
	}

	/**
	 * Clears all entries from the {@link GameListAdapter}
	 * backing this GameListFragment.
	 */
	public void clearGameList()
	{
		mGameAdapter.clear();
		mGameAdapter.notifyDataSetChanged();
	}

	private void fill()
	{
		List<GameListItem> fls = new ArrayList<GameListItem>();
		String Directories = NativeLibrary.GetConfig("Dolphin.ini", "General", "ISOPaths", "0");
		int intDirectories = Integer.parseInt(Directories);

		// Extensions to filter by.
		Set<String> exts = new HashSet<String>(Arrays.asList(".dff", ".dol", ".elf", ".gcm", ".gcz", ".iso", ".wad", ".wbfs"));

		for (int a = 0; a < intDirectories; ++a)
		{
			String BrowseDir = NativeLibrary.GetConfig("Dolphin.ini", "General", "ISOPath" + a, "");
			File currentDir = new File(BrowseDir);
			File[] dirs = currentDir.listFiles();
			try
			{
				for (File entry : dirs)
				{
					String entryName = entry.getName();

					if (!entry.isHidden() && !entry.isDirectory())
					{
						if (exts.contains(entryName.toLowerCase().substring(entryName.lastIndexOf('.'))))
							fls.add(new GameListItem(getActivity(), entryName, entry.length(), entry.getAbsolutePath()));
					}

				}
			}
			catch (Exception ignored)
			{
			}
		}
		Collections.sort(fls);

		// Add all the items to the adapter
		mGameAdapter.addAll(fls);
		mGameAdapter.notifyDataSetChanged();

		if (fls.isEmpty())
		{
			mCallback.onZeroFiles();
		}
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		ListView rootView = (ListView) inflater.inflate(R.layout.gamelist_listview, container, false);
		mGameAdapter = new GameListAdapter(getActivity(), R.layout.gamelist_list_item);
		rootView.setAdapter(mGameAdapter);

		fill();

		return rootView;
	}
	
	@Override
	public void onListItemClick(ListView listView, View view, int position, long id)
	{
		GameListItem item = mGameAdapter.getItem(position);

		// Show a toast indicating which game was clicked.
		Toast.makeText(getActivity(), String.format(getString(R.string.file_clicked), item.getPath()), Toast.LENGTH_SHORT).show();
		
		Notification note=new Notification(R.drawable.stat_notify_chat,
                                          "Dolphin Emulator",
                                          System.currentTimeMillis());
		
		Intent i=new Intent(this, GameListActivity.class);
    
		i.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP|
				Intent.FLAG_ACTIVITY_SINGLE_TOP);
		
		PendingIntent pi=PendingIntent.getActivity(this, 0,
                                                  i, 0);
      
		note.setLatestEventInfo(this, "Dolphin Emulator",
                              "Running",
                              pi);
		note.flags|=Notification.FLAG_NO_CLEAR;

		// Start the emulation activity and send the path of the clicked ROM to it.
		Intent intent = new Intent(getActivity(), EmulationActivity.class);
		intent.putExtra("SelectedGame", item.getPath());
		startActivity(intent);
		startForeground(1337, note);
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
		}
		catch (ClassCastException e)
		{
			throw new ClassCastException(activity.toString()
					+ " must implement OnGameListZeroListener");
		}
	}
}
