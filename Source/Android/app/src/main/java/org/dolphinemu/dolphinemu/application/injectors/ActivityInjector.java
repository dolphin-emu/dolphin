package org.dolphinemu.dolphinemu.application.injectors;


import org.dolphinemu.dolphinemu.application.DolphinApplication;
import org.dolphinemu.dolphinemu.ui.main.MainActivity;
import org.dolphinemu.dolphinemu.ui.main.MainModule;
import org.dolphinemu.dolphinemu.ui.main.TvMainActivity;
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

	private ActivityInjector()
	{
	}
}
