package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

public final class StartupHandler
{
	/**
	 * @param assetManager An AssetManager that will be valid (won't get garbage collected)
	 *                     for the whole lifetime of the application.
	 */
	public static boolean HandleInit(Activity parent, AssetManager assetManager)
	{
		NativeLibrary.SetAssetManager(assetManager);

		Intent intent = parent.getIntent();
		Bundle extras = intent.getExtras();

		String user_dir = "";
		if (extras != null)
			user_dir = extras.getString("UserDir");

		// If user_dir is an empty string, SetUserDirectory will use the default user directory
		NativeLibrary.SetUserDirectory(user_dir);

		if (PermissionsHandler.checkWritePermission(parent)) {
			NativeLibrary.CreateUserFolders();
		}

		if (extras != null)
		{
			String start_file = extras.getString("AutoStartFile");
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
