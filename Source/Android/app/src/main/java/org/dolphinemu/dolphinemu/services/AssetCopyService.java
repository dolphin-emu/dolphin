/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.utils.Log;

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
			Log.verbose("[AssetCopyService] Skipping asset copy operation.");
		}

		// Always copy over the GCPad config in case of change or corruption.
		// Not a user configurable file.
		copyAsset("GCPadNew.ini", ConfigDir + File.separator + "GCPadNew.ini");
		copyAsset("WiimoteNew.ini", ConfigDir + File.separator + "WiimoteNew.ini");

		// Record the fact that we've done this before, so we don't do it on every launch.
		SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
		SharedPreferences.Editor editor = preferences.edit();

		editor.putBoolean("assetsCopied", true);
		editor.commit();
	}

	private void copyAsset(String asset, String output)
	{
		Log.verbose("[AssetCopyService] Copying " + asset + " to " + output);
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
			Log.error("[AssetCopyService] Failed to copy asset file: " + asset + e.getMessage());
		}
	}

	private void copyAssetFolder(String assetFolder, String outputFolder)
	{
		Log.verbose("[AssetCopyService] Copying " + assetFolder + " to " + outputFolder);

		try
		{
			for (String file : getAssets().list(assetFolder))
			{
				copyAsset(assetFolder + File.separator + file, outputFolder + File.separator + file);
			}
		}
		catch (IOException e)
		{
			Log.error("[AssetCopyService] Failed to copy asset folder: " + assetFolder + e.getMessage());
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
