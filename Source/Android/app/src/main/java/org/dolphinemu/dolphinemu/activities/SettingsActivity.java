package org.dolphinemu.dolphinemu.activities;


import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;

import org.dolphinemu.dolphinemu.fragments.SettingsFragment;
import org.dolphinemu.dolphinemu.services.SettingsSaveService;

public final class SettingsActivity extends AppCompatActivity
{
	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Display the fragment as the main content.
		getFragmentManager().beginTransaction()
				.replace(android.R.id.content, new SettingsFragment(), "settings_fragment")
				.commit();
	}

	/**
	 * If this is called, the user has left the settings screen (potentially through the
	 * home button) and will expect their changes to be persisted. So we kick off an
	 * IntentService which will do so on a background thread.
	 */
	@Override
	protected void onStop()
	{
		super.onStop();

		Log.d("DolphinEmulator", "Settings activity stopping. Saving settings to INI...");

		// Copy assets into appropriate locations.
		Intent settingsSaver = new Intent(this, SettingsSaveService.class);
		startService(settingsSaver);
	}

	public static void launch(Context context)
	{
		Intent settings = new Intent(context, SettingsActivity.class);
		context.startActivity(settings);
	}
}
