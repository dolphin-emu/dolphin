package org.dolphinemu.dolphinemu;

import android.app.Application;

import org.dolphinemu.dolphinemu.model.GameDatabase;

public class DolphinApplication extends Application
{
	public static GameDatabase databaseHelper;

	@Override
	public void onCreate()
	{
		super.onCreate();

		databaseHelper = new GameDatabase(this);
	}
}
