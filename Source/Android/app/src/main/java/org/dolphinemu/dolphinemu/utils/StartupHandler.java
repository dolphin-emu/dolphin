package org.dolphinemu.dolphinemu.utils;

import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.activities.EmulationActivity;

public final class StartupHandler
{
	public static void HandleInit(FragmentActivity parent)
	{
		// Ask the user to grant write permission if it's not already granted
		PermissionsHandler.checkWritePermission(parent);

		String start_file = "";
		Bundle extras = parent.getIntent().getExtras();
		if (extras != null)
			start_file = extras.getString("AutoStartFile");

		if (!TextUtils.isEmpty(start_file))
		{
			// Start the emulation activity, send the ISO passed in and finish the main activity
			Intent emulation_intent = new Intent(parent, EmulationActivity.class);
			emulation_intent.putExtra("SelectedGame", start_file);
			parent.startActivity(emulation_intent);
			parent.finish();
		}
	}
}
