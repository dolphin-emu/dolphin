package org.dolphinemu.dolphinemu;

import android.app.Application;

import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;

public class DolphinApplication extends Application
{
	public static GameDatabase databaseHelper;

	@Override
	public void onCreate()
	{
		super.onCreate();

		if (PermissionsHandler.hasWriteAccess(getApplicationContext()))
			DirectoryInitializationService.startService(getApplicationContext());

		databaseHelper = new GameDatabase(this);
	}
}
