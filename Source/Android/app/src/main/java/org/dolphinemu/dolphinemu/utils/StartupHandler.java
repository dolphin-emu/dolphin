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
		NativeLibrary.SetUserDirectory(""); // Auto-Detect

		if (PermissionsHandler.checkWritePermission(parent)) {
			DirectoryInitializationService.startService(parent);
		}

		Intent intent = parent.getIntent();
		Bundle extras = intent.getExtras();

		if (extras != null)
		{
			String user_dir = extras.getString("UserDir");
			String start_file = extras.getString("AutoStartFile");

			if (!TextUtils.isEmpty(user_dir))
				NativeLibrary.SetUserDirectory(user_dir);

			if (!TextUtils.isEmpty(start_file))
			{
				// Start the emulation activity, send the ISO passed in and finish the main activity
				Intent emulation_intent = new Intent(parent, EmulationActivity.class);
				emulation_intent.putExtra("SelectedGame", start_file);
				parent.startActivity(emulation_intent);
				parent.finish();
				return false;
			}
		}
		return false;
	}
}
