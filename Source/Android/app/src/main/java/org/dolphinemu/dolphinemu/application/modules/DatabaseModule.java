package org.dolphinemu.dolphinemu.application.modules;

import android.content.Context;

import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.utils.Log;

import javax.inject.Singleton;

import dagger.Module;
import dagger.Provides;

@Module
public class DatabaseModule
{
	@Provides
	@Singleton
	GameDatabase provideDatabase(Context context)
	{
		Log.verbose("[DatabaseModule] Providing GameDatabase...");
		return new GameDatabase(context);
	}
}
