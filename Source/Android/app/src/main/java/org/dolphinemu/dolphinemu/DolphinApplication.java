package org.dolphinemu.dolphinemu;

import android.app.Application;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.VolleyUtil;

public class DolphinApplication extends Application
{
	public static final String FIRST_OPEN = "FIRST_OPEN";
	@Override
	public void onCreate()
	{
		super.onCreate();

		// Passed at emulation start to trigger first open event.
		SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
		SharedPreferences.Editor sPrefsEditor = preferences.edit();
		sPrefsEditor.putBoolean(FIRST_OPEN, true);
		sPrefsEditor.apply();

		VolleyUtil.init(getApplicationContext());
		System.loadLibrary("main");

		if (PermissionsHandler.hasWriteAccess(getApplicationContext()))
			DirectoryInitializationService.startService(getApplicationContext());
	}
}
