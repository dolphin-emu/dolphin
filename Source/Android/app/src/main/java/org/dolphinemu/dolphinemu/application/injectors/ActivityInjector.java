package org.dolphinemu.dolphinemu.application.injectors;


import org.dolphinemu.dolphinemu.application.DolphinApplication;
import org.dolphinemu.dolphinemu.ui.files.AddDirectoryActivity;
import org.dolphinemu.dolphinemu.ui.files.AddDirectoryModule;
import org.dolphinemu.dolphinemu.ui.main.MainActivity;
import org.dolphinemu.dolphinemu.ui.main.MainModule;
import org.dolphinemu.dolphinemu.ui.main.tv.TvMainActivity;
import org.dolphinemu.dolphinemu.utils.Log;

public class ActivityInjector
{
	public static void inject(MainActivity activity)
	{
		Log.verbose("[ActivityInjector] Injecting MainActivity.");

		DolphinApplication.appComponent
				.plus(new MainModule(activity))
				.inject(activity);
	}

	public static void inject(TvMainActivity activity)
	{
		Log.verbose("[ActivityInjector] Injecting TvMainActivity.");

		DolphinApplication.appComponent
				.plus(new MainModule(activity))
				.inject(activity);
	}

	public static void inject(AddDirectoryActivity activity)
	{
		Log.verbose("[ActivityInjector] Injecting AddDirectoryActivity.");

		DolphinApplication.appComponent
				.plus(new AddDirectoryModule(activity))
				.inject(activity);
	}

	private ActivityInjector()
	{
	}
}
