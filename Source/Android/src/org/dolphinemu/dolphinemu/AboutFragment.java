/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

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

import org.dolphinemu.dolphinemu.folderbrowser.FolderBrowserAdapter;
import org.dolphinemu.dolphinemu.folderbrowser.FolderBrowserItem;
import org.dolphinemu.dolphinemu.settings.VideoSettingsFragment;


public final class AboutFragment extends Fragment
{
	private static Activity m_activity;
	private ListView mMainList;
	private FolderBrowserAdapter adapter;

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		View rootView = inflater.inflate(R.layout.gamelist_listview, container, false);
		mMainList = (ListView) rootView.findViewById(R.id.gamelist);
		
		String yes = getString(R.string.yes);
		String no = getString(R.string.no);

		List<FolderBrowserItem> Input = new ArrayList<FolderBrowserItem>();
		Input.add(new FolderBrowserItem(getString(R.string.build_revision), NativeLibrary.GetVersionString(), ""));
		Input.add(new FolderBrowserItem(getString(R.string.supports_gles3), VideoSettingsFragment.SupportsGLES3() ? yes : no, ""));

		adapter = new FolderBrowserAdapter(m_activity, R.layout.about_layout, Input);
		mMainList.setAdapter(adapter);

		return mMainList;
	}
	
	@Override
	public void onAttach(Activity activity)
	{
		super.onAttach(activity);

		// Cache the activity instance.
		m_activity = activity;
	}
}
