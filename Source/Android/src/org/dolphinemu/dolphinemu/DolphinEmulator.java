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
import java.nio.channels.FileChannel;

/**
 * The main activity of this emulator front-end.
 */
public final class DolphinEmulator extends Activity 
{
	private void CopyAsset(String asset, String output)
	{
		try
		{
			// Get input file channel.
			FileInputStream inStream = getAssets().openFd(asset).createInputStream();
			FileChannel in =  inStream.getChannel();

			// Get output file channel.
			FileOutputStream outStream = new FileOutputStream(output);
			FileChannel out = outStream.getChannel();

			// Copy over
			in.transferTo(0, in.size(), out);

			// Clean-up
			inStream.close();
			outStream.close();
		}
		catch (IOException e)
		{
			Log.e("DolphinEmulator", "Failed to copy asset file: " + asset, e);
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
				CopyAsset("ButtonA.png",     BaseDir + File.separator + "ButtonA.png");
				CopyAsset("ButtonB.png",     BaseDir + File.separator + "ButtonB.png");
				CopyAsset("ButtonStart.png", BaseDir + File.separator + "ButtonStart.png");
				CopyAsset("NoBanner.png",    BaseDir + File.separator + "NoBanner.png");
				CopyAsset("GCPadNew.ini",    ConfigDir + File.separator + "GCPadNew.ini");
				CopyAsset("Dolphin.ini",     ConfigDir + File.separator + "Dolphin.ini");
				CopyAsset("dsp_coef.bin",    GCDir + File.separator + "dsp_coef.bin");
				CopyAsset("dsp_rom.bin",     GCDir + File.separator + "dsp_rom.bin");
				CopyAsset("font_ansi.bin",   GCDir + File.separator + "font_ansi.bin");
				CopyAsset("font_sjis.bin",   GCDir + File.separator + "font_sjis.bin");
			}

			// Load the configuration keys set in the Dolphin ini and gfx ini files
			// into the application's shared preferences.
			UserPreferences.LoadIniToPrefs(this);
		}
	}
}