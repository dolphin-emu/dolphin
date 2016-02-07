package org.dolphinemu.dolphinemu.application.modules;


import android.content.Context;

import org.dolphinemu.dolphinemu.application.DolphinApplication;
import org.dolphinemu.dolphinemu.utils.Log;

import javax.inject.Singleton;

import dagger.Module;
import dagger.Provides;

@Module
public class AppModule
{
	private DolphinApplication application;

	public AppModule(DolphinApplication application)
	{
		this.application = application;
	}

	@Provides
	@Singleton
	public Context provideApplication()
	{
		Log.verbose("[AppModule] Providing Application...");
		return application;
	}
}
