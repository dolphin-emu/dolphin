package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Intent;

import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.UserPreferences;

/**
 * IntentServices, unlike regular services, inherently run on a background thread.
 * This IntentService saves all the options the user set in the Java-based UI into
 * INI files the native code can read.
 */
public final class SettingsSaveService extends IntentService
{
	public SettingsSaveService()
	{
		super("SettingsSaveService");
	}

	@Override
	protected void onHandleIntent(Intent intent)
	{
		Log.verbose("[SettingsSaveService] Saving settings to INI files...");
		UserPreferences.SavePrefsToIni(this);
		Log.verbose("[SettingsSaveService] Save successful.");
	}
}
