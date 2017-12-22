/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
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
 * A service that spawns its own thread in order to copy several binary and shader files
 * from the Dolphin APK to the external file system.
 */
public final class DirectoryInitializationService extends IntentService
{
    public static final String BROADCAST_ACTION = "org.dolphinemu.dolphinemu.BROADCAST";

    public static final String EXTRA_STATE = "directoryState";
    private static DirectoryInitializationState directoryState = null;

    public enum DirectoryInitializationState
    {
        DOLPHIN_DIRECTORIES_INITIALIZED,
        EXTERNAL_STORAGE_PERMISSION_NEEDED
    }

    public DirectoryInitializationService()
    {
        // Superclass constructor is called to name the thread on which this service executes.
        super("DirectoryInitializationService");
    }

    public static void startService(Context context)
    {
        Intent intent = new Intent(context, DirectoryInitializationService.class);
        context.startService(intent);
    }

    @Override
    protected void onHandleIntent(Intent intent)
    {
        if (directoryState == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
        {
            sendBroadcastState(DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED);
        }
        else if (PermissionsHandler.hasWriteAccess(this))
        {
            initDolphinDirectories();
            directoryState = DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED;
            sendBroadcastState(directoryState);
        }
        else
        {
            sendBroadcastState(DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED);
        }
    }

    private void initDolphinDirectories()
    {
        String BaseDir = NativeLibrary.GetUserDirectory();
        String ConfigDir = BaseDir + File.separator + "Config";

        // Copy assets if needed
        NativeLibrary.CreateUserFolders();
        copyAssetFolder("GC", BaseDir + File.separator + "GC", false);
        copyAssetFolder("Shaders", BaseDir + File.separator + "Shaders", false);
        copyAssetFolder("Wii", BaseDir + File.separator + "Wii", false);

        // Always copy over the GCPad config in case of change or corruption.
        // Not a user configurable file.
        copyAsset("GCPadNew.ini", ConfigDir + File.separator + "GCPadNew.ini", true);
        copyAsset("WiimoteNew.ini", ConfigDir + File.separator + "WiimoteNew.ini", false);
    }

    public static boolean areDolphinDirectoriesReady()
    {
        return directoryState == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED;
    }

    private void sendBroadcastState(DirectoryInitializationState state)
    {
        Intent localIntent =
                new Intent(BROADCAST_ACTION)
                        .putExtra(EXTRA_STATE, state);
        LocalBroadcastManager.getInstance(this).sendBroadcast(localIntent);
    }

    private void copyAsset(String asset, String output, Boolean overwrite)
    {
        Log.verbose("[DirectoryInitializationService] Copying File " + asset + " to " + output);
        InputStream in;
        OutputStream out;

        try
        {
            File file = new File(output);
            if (!file.exists() || overwrite)
            {
                in = getAssets().open(asset);
                out = new FileOutputStream(output);
                copyFile(in, out);
                in.close();
                out.close();
            }
        }
        catch (IOException e)
        {
            Log.error("[DirectoryInitializationService] Failed to copy asset file: " + asset + e.getMessage());
        }
    }

    private void copyAssetFolder(String assetFolder, String outputFolder, Boolean overwrite)
    {
        Log.verbose("[DirectoryInitializationService] Copying Folder " + assetFolder + " to " + outputFolder);

        try
        {
            for (String file : getAssets().list(assetFolder))
            {
                copyAssetFolder(assetFolder + File.separator + file, outputFolder + File.separator + file, overwrite);
                copyAsset(assetFolder + File.separator + file, outputFolder + File.separator + file, overwrite);
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
}
