/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Intent;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.CountDownLatch;

/**
 * A service that spawns its own thread in order to create directories and extract files.
 */
public final class DirectoryInitializationService extends IntentService
{
	private static volatile boolean sServiceStarted = false;
	private static CountDownLatch sInternalStorageInitialized = new CountDownLatch(1);
	private static CountDownLatch sExternalStorageInitialized = new CountDownLatch(1);
	private static CountDownLatch sHasExternalStoragePermission = new CountDownLatch(1);

	private static void waitFor(CountDownLatch latch)
	{
		while (true)
		{
			try
			{
				latch.await();
				return;
			}
			catch (InterruptedException e)
			{
			}
		}
	}

	public static void waitForInternalStorageInitialization()
	{
		// The exception we might throw in here is intended to make us notice logic errors that
		// could've led to waiting for something that never will happen.

		if (!sServiceStarted)
			throw new IllegalStateException("DirectoryInitializationService not started");

		waitFor(sInternalStorageInitialized);
	}

	public static void waitForExternalStorageInitialization()
	{
		// The exceptions we might throw in here are intended to make us notice logic errors that
		// otherwise could've led to waiting for something that never will happen.

		if (!sServiceStarted)
			throw new IllegalStateException("DirectoryInitializationService not started");

		if (sHasExternalStoragePermission.getCount() != 0)
			throw new IllegalStateException("External storage write permission not granted");

		waitFor(sExternalStorageInitialized);
	}

	public static void setHasExternalStoragePermission()
	{
		sHasExternalStoragePermission.countDown();
	}

	public DirectoryInitializationService()
	{
		// Superclass constructor is called to name the thread on which this service executes.
		super("DirectoryInitializationService");
	}

	@Override
	protected void onHandleIntent(Intent intent)
	{
		if (sServiceStarted)
			throw new IllegalStateException("DirectoryInitializationService shouldn't run twice");
		sServiceStarted = true;

		initializeInternalStorage();
		sInternalStorageInitialized.countDown();

		waitFor(sHasExternalStoragePermission);
		initializeExternalStorage();
		sExternalStorageInitialized.countDown();
	}

	private void initializeInternalStorage()
	{
		File sysDirectory = new File(getFilesDir(), "Sys");

		// Delete the existing extracted Sys directory in case it's from a different version of Dolphin.
		deleteDirectoryRecursively(sysDirectory);

		// Extract the Sys directory to app-local internal storage.
		copyAssetFolder("Sys", sysDirectory);

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

	private static void deleteDirectoryRecursively(File file)
	{
		if (file.isDirectory())
		{
			for (File child : file.listFiles())
				deleteDirectoryRecursively(child);
		}

		file.delete();
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
