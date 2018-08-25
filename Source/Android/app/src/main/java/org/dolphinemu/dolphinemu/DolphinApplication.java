package org.dolphinemu.dolphinemu;

import android.app.Application;

import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.VolleyUtil;

public class DolphinApplication extends Application
{
	@Override
	public void onCreate()
	{
		super.onCreate();

		VolleyUtil.init(getApplicationContext());
		System.loadLibrary("main");

		if (PermissionsHandler.hasWriteAccess(getApplicationContext()))
			DirectoryInitializationService.startService(getApplicationContext());
	}
}
