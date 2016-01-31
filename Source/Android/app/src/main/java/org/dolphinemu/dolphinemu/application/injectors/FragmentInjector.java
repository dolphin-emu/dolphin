package org.dolphinemu.dolphinemu.application.injectors;


import org.dolphinemu.dolphinemu.application.DolphinApplication;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesFragment;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesModule;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesView;
import org.dolphinemu.dolphinemu.utils.Log;

public class FragmentInjector
{

	public static void inject(PlatformGamesView view)
	{
		Log.verbose("[ActivityInjector] Injecting GameListView.");

		DolphinApplication.appComponent
				.plus(new PlatformGamesModule(view))
				.inject((PlatformGamesFragment) view);
	}

	private FragmentInjector()
	{
	}
}
