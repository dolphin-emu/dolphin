package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.app.Fragment;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import java.util.ArrayList;
import java.util.List;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class AboutFragment extends Fragment {
	private static Activity m_activity;

	private ListView mMainList;

	private FolderBrowserAdapter adapter;
	private int configPosition = 0;
	boolean Configuring = false;
	boolean firstEvent = true;

	public AboutFragment() {
		// Empty constructor required for fragment subclasses
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
	                         Bundle savedInstanceState) {
		View rootView = inflater.inflate(R.layout.gamelist_listview, container, false);
		mMainList = (ListView) rootView.findViewById(R.id.gamelist);

		List<GameListItem> Input = new ArrayList<GameListItem>();
		int a = 0;

		Input.add(a++, new GameListItem(m_activity, "Build Revision", NativeLibrary.GetVersionString(), "", true));
		Input.add(a++, new GameListItem(m_activity, "Supports OpenGL ES 3", PrefsFragment.SupportsGLES3() ? "Yes" : "No", "", true));
		adapter = new FolderBrowserAdapter(m_activity, R.layout.folderbrowser, Input);
		mMainList.setAdapter(adapter);

		return mMainList;
	}
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
}
