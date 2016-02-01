package org.dolphinemu.dolphinemu.application.injectors;


import org.dolphinemu.dolphinemu.application.DolphinApplication;
import org.dolphinemu.dolphinemu.ui.files.FileListFragment;
import org.dolphinemu.dolphinemu.ui.files.FileListModule;
import org.dolphinemu.dolphinemu.ui.files.FileListView;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesFragment;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesModule;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesView;
import org.dolphinemu.dolphinemu.utils.Log;

public class FragmentInjector
{

	public static void inject(PlatformGamesView view)
	{
		Log.verbose("[ActivityInjector] Injecting PlatformGamesView.");

		DolphinApplication.appComponent
				.plus(new PlatformGamesModule(view))
				.inject((PlatformGamesFragment) view);
	}

	public static void inject(FileListView view)
	{
		Log.verbose("[ActivityInjector] Injecting FileListFragment.");

		DolphinApplication.appComponent
				.plus(new FileListModule(view))
				.inject((FileListFragment) view);
	}

	private FragmentInjector()
	{
	}
}
