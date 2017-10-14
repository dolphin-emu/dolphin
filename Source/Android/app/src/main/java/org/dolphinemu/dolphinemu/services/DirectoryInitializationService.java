/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.support.v4.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * A service that spawns its own thread in order to create directories and extract files.
 */
public final class DirectoryInitializationService extends IntentService
{
	public static final String BROADCAST_ACTION = "org.dolphinemu.dolphinemu.BROADCAST";

	public static final String EXTRA_STATE = "directoryState";
	private static DirectoryInitializationState directoryState = null;

	public enum DirectoryInitializationState {
		DOLPHIN_DIRECTORIES_INITIALIZED,
		EXTERNAL_STORAGE_PERMISSION_NEEDED
	}

	public DirectoryInitializationService()
	{
		// Superclass constructor is called to name the thread on which this service executes.
		super("DirectoryInitializationService");
	}

	public static void startService(Context context) {
		Intent intent = new Intent(context, DirectoryInitializationService.class);
		context.startService(intent);
	}

	@Override
	protected void onHandleIntent(Intent intent)
	{
		initializeInternalStorage();
		if (PermissionsHandler.hasWriteAccess(this))
		{
			initializeExternalStorage();
			directoryState = DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED;
			sendBroadcastState(DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED);
		}
		else
		{
			sendBroadcastState(DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED);
		}
	}

	private void initializeInternalStorage()
	{
		File sysDirectory = new File(getFilesDir(), "Sys");

		SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
		String revision = NativeLibrary.GetGitRevision();
		if (!preferences.getString("sysDirectoryVersion", "").equals(revision))
		{
			// There is no extracted Sys directory, or there is a Sys directory from another
			// version of Dolphin that might contain outdated files. Let's (re-)extract Sys.
			deleteDirectoryRecursively(sysDirectory);
			copyAssetFolder("Sys", sysDirectory);

			SharedPreferences.Editor editor = preferences.edit();
			editor.putString("sysDirectoryVersion", revision);
			editor.apply();
		}

		// Let the native code know where the Sys directory is.
		SetSysDirectory(sysDirectory.getPath());
	}

	private void initializeExternalStorage()
	{
		// Create User directory structure and copy some NAND files from the extracted Sys directory.
		CreateUserDirectories();

		// Always copy over the controller config INIs in case of change or corruption.
		// Not user configurable files.
		// TODO: Requiring fixed files in Config is a bit silly - the directory is for user config
		String configDirectory = NativeLibrary.GetUserDirectory() + File.separator + "Config";
		copyAsset("GCPadNew.ini", new File(configDirectory, "GCPadNew.ini"));
		copyAsset("WiimoteNew.ini", new File(configDirectory, "WiimoteNew.ini"));
	}

	public static boolean isDolphinDirectoriesReady() {
		return directoryState == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED;
	}

	private static void deleteDirectoryRecursively(File file)
	{
		if (file.isDirectory())
		{
			for (File child : file.listFiles())
				deleteDirectoryRecursively(child);
		}

		file.delete();
	}

	private void sendBroadcastState(DirectoryInitializationState state) {
		Intent localIntent =
				new Intent(BROADCAST_ACTION)
						.putExtra(EXTRA_STATE, state);
		LocalBroadcastManager.getInstance(this).sendBroadcast(localIntent);
	}

	private void copyAsset(String asset, File output)
	{
		Log.verbose("[DirectoryInitializationService] Copying File " + asset + " to " + output);
		InputStream in;
		OutputStream out;

		try
		{
			in = getAssets().open(asset);
			output.createNewFile();
			out = new FileOutputStream(output);
			copyFile(in, out);
			in.close();
			out.close();
		}
		catch (IOException e)
		{
			Log.error("[DirectoryInitializationService] Failed to copy asset file: " + asset + e.getMessage());
		}
	}

	private void copyAssetFolder(String assetFolder, File outputFolder)
	{
		Log.verbose("[DirectoryInitializationService] Copying Folder " + assetFolder + " to " + outputFolder);

		try
		{
			boolean createdFolder = false;
			for (String file : getAssets().list(assetFolder))
			{
				if (!createdFolder)
				{
					outputFolder.mkdir();
					createdFolder = true;
				}
				copyAssetFolder(assetFolder + File.separator + file, new File(outputFolder, file));
				copyAsset(assetFolder + File.separator + file, new File(outputFolder, file));
			}
		}
		catch (IOException e)
		{
			Log.error("[DirectoryInitializationService] Failed to copy asset folder: " + assetFolder + e.getMessage());
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

	private static native void CreateUserDirectories();
	private static native void SetSysDirectory(String path);
}
