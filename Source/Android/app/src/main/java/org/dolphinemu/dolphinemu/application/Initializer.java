package org.dolphinemu.dolphinemu.application;

import org.dolphinemu.dolphinemu.application.modules.AppModule;
import org.dolphinemu.dolphinemu.utils.Log;

public class Initializer
{

	public static AppComponent initAppComponent(DolphinApplication application)
	{
		Log.verbose("[Initializer] Initializing Dagger AppComponent.");
		return DaggerAppComponent
				.builder()
				.appModule(new AppModule(application))
				.build();
	}

	private Initializer()
	{
	}
}
