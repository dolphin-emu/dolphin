package org.dolphinemu.dolphinemu.utils;

import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;

public final class StartupHandler
{
	public static boolean HandleInit(FragmentActivity parent)
	{
		String user_dir = "";
		String start_file = "";

		Bundle extras = parent.getIntent().getExtras();
		if (extras != null)
		{
			user_dir = extras.getString("UserDir");
			start_file = extras.getString("AutoStartFile");
		}

		NativeLibrary.SetUserDirectory(user_dir);  // Uses default path if user_dir equals ""

		if (PermissionsHandler.checkWritePermission(parent))
			DirectoryInitializationService.startService(parent);

		if (!TextUtils.isEmpty(start_file))
		{
			// Start the emulation activity, send the ISO passed in and finish the main activity
			Intent emulation_intent = new Intent(parent, EmulationActivity.class);
			emulation_intent.putExtra("SelectedGame", start_file);
			parent.startActivity(emulation_intent);
			parent.finish();
			return false;
		}

		return false;
	}
}
