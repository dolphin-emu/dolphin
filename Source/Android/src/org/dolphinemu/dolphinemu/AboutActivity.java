package org.dolphinemu.dolphinemu;

import android.app.ListActivity;
import android.os.Bundle;

import java.util.ArrayList;
import java.util.List;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class AboutActivity extends ListActivity {
	private FolderBrowserAdapter adapter;
	private int configPosition = 0;
	boolean Configuring = false;
	boolean firstEvent = true;

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		List<GameListItem> Input = new ArrayList<GameListItem>();
		int a = 0;

		Input.add(a++, new GameListItem(getApplicationContext(), "Build Revision", NativeLibrary.GetVersionString(), "", true));
		adapter = new FolderBrowserAdapter(this, R.layout.folderbrowser, Input);
		setListAdapter(adapter);
	}
}
