/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Intent;
import android.util.Log;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.settings.UserPreferences;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * A service that spawns its own thread in order to copy several binary and shader files
 * from the Dolphin APK to the external file system.
 */
public final class AssetCopyService extends IntentService
{
	private static final String TAG = "DolphinEmulator";

	public AssetCopyService()
	{
		// Superclass constructor is called to name the thread on which this service executes.
		super("AssetCopyService");
	}

	@Override
	protected void onHandleIntent(Intent intent)
	{
		String BaseDir = NativeLibrary.GetUserDirectory();
		String ConfigDir = BaseDir + File.separator + "Config";
		String GCDir = BaseDir + File.separator + "GC";

		// Copy assets if needed
		File file = new File(GCDir + File.separator + "font_sjis.bin");
		if(!file.exists())
		{
			NativeLibrary.CreateUserFolders();
			copyAsset("dsp_coef.bin", GCDir + File.separator + "dsp_coef.bin");
			copyAsset("dsp_rom.bin", GCDir + File.separator + "dsp_rom.bin");
			copyAsset("font_ansi.bin", GCDir + File.separator + "font_ansi.bin");
			copyAsset("font_sjis.bin", GCDir + File.separator + "font_sjis.bin");
			copyAssetFolder("Shaders", BaseDir + File.separator + "Shaders");
		}
		else
		{
			Log.v(TAG, "Skipping asset copy operation.");
		}

		// Always copy over the GCPad config in case of change or corruption.
		// Not a user configurable file.
		copyAsset("GCPadNew.ini", ConfigDir + File.separator + "GCPadNew.ini");

		// Load the configuration keys set in the Dolphin ini and gfx ini files
		// into the application's shared preferences.
		UserPreferences.LoadIniToPrefs(this);
	}

	private void copyAsset(String asset, String output)
	{
		Log.v(TAG, "Copying " + asset + " to " + output);
		InputStream in = null;
		OutputStream out = null;

		try
		{
			in = getAssets().open(asset);
			out = new FileOutputStream(output);
			copyFile(in, out);
			in.close();
			out.close();
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to copy asset file: " + asset, e);
		}
	}

	private void copyAssetFolder(String assetFolder, String outputFolder)
	{
		Log.v(TAG, "Copying " + assetFolder + " to " + outputFolder);

		try
		{
			for (String file : getAssets().list(assetFolder))
			{
				copyAsset(assetFolder + File.separator + file, outputFolder + File.separator + file);
			}
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to copy asset folder: " + assetFolder, e);
		}
	}

	private void copyFile(InputStream in, OutputStream out) throws IOException
	{
		byte[] buffer = new byte[1024];
		int read;

		while ((read = in.read(buffer)) != -1)
		{
			out.write(buffer, 0, read);
		}
	}
}
