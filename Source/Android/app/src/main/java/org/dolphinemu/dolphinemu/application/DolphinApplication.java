package org.dolphinemu.dolphinemu.application;

import android.app.Application;

import org.dolphinemu.dolphinemu.utils.Log;


public class DolphinApplication extends Application
{
	public static AppComponent appComponent;

	@Override
	public void onCreate()
	{
		super.onCreate();

		appComponent = Initializer.initAppComponent(this);
		if (appComponent != null)
		{
			Log.verbose("[DolphinApplication] Initialized Dagger AppComponent.");
		}
	}
}
