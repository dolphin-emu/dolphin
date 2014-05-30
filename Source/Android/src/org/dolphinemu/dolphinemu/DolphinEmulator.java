/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import org.dolphinemu.dolphinemu.gamelist.GameListActivity;
import org.dolphinemu.dolphinemu.settings.UserPreferences;

import java.io.*;

/**
 * The main activity of this emulator front-end.
 */
public final class DolphinEmulator extends Activity 
{
	private void CopyAsset(String asset, String output)
	{
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
			Log.e("DolphinEmulator", "Failed to copy asset file: " + asset, e);
		}
	}

	private void CopyAssetFolder(String assetFolder, String outputFolder)
	{
		try
		{
			for (String file : getAssets().list(assetFolder))
			{
				CopyAsset(assetFolder + File.separator + file, outputFolder + File.separator + file);
			}
		}
		catch (IOException e)
		{
			Log.e("DolphinEmulator", "Failed to copy asset folder: " + assetFolder, e);
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

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		if (savedInstanceState == null)
		{
			Intent GameListIntent = new Intent(this, GameListActivity.class);
			startActivity(GameListIntent);

			String BaseDir = Environment.getExternalStorageDirectory()+File.separator+"dolphin-emu";
			String ConfigDir = BaseDir + File.separator + "Config";
			String GCDir = BaseDir + File.separator + "GC";

			// Copy assets if needed
			File file = new File(GCDir + File.separator + "font_sjis.bin");
			if(!file.exists())
			{
				NativeLibrary.CreateUserFolders();
				CopyAsset("Dolphin.ini",     ConfigDir + File.separator + "Dolphin.ini");
				CopyAsset("dsp_coef.bin",    GCDir + File.separator + "dsp_coef.bin");
				CopyAsset("dsp_rom.bin",     GCDir + File.separator + "dsp_rom.bin");
				CopyAsset("font_ansi.bin",   GCDir + File.separator + "font_ansi.bin");
				CopyAsset("font_sjis.bin",   GCDir + File.separator + "font_sjis.bin");
				CopyAssetFolder("Shaders", BaseDir + File.separator + "Shaders");
			}

			// Always copy over the GCPad config in case of change or corruption.
			// Not a user configurable file.
			CopyAsset("GCPadNew.ini",    ConfigDir + File.separator + "GCPadNew.ini");

			// Load the configuration keys set in the Dolphin ini and gfx ini files
			// into the application's shared preferences.
			UserPreferences.LoadIniToPrefs(this);
		}
	}

	@Override
	protected void onRestart()
	{
		super.onRestart();
		finish(); // If we are ever returning to this activity then we are exiting.
	}
}
