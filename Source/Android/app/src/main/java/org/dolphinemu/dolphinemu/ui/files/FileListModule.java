package org.dolphinemu.dolphinemu.ui.files;


import org.dolphinemu.dolphinemu.application.scopes.FragmentScoped;
import org.dolphinemu.dolphinemu.utils.Log;

import dagger.Module;
import dagger.Provides;

@Module
public class FileListModule
{
	private FileListView view;

	public FileListModule(FileListView view)
	{
		this.view = view;
	}

	@Provides
	@FragmentScoped
	public FileListView provideView()
	{
		Log.verbose("[FileListModule] Providing view...");
		return view;
	}
}
