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
            initializeInternalStorage();
            initializeExternalStorage();
            directoryState = DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED;
            sendBroadcastState(directoryState);
        }
        else
        {
            sendBroadcastState(DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED);
        }
    }

    private void initializeInternalStorage()
    {
        File sysDirectory = new File(getFilesDir(), "Sys");

        // Delete the existing extracted Sys directory in case it's from a different version of Dolphin.
        deleteDirectoryRecursively(sysDirectory);

        // Extract the Sys directory to app-local internal storage.
        copyAssetFolder("Sys", sysDirectory, true);

        // Let the native code know where the Sys directory is.
        SetSysDirectory(sysDirectory.getPath());
    }

    private void initializeExternalStorage()
    {
        // Create User directory structure and copy some NAND files from the extracted Sys directory.
        CreateUserDirectories();

        // GCPadNew.ini and WiimoteNew.ini must contain specific values in order for controller
        // input to work as intended (they aren't user configurable), so we overwrite them just
        // in case the user has tried to modify them manually.
        //
        // ...Except WiimoteNew.ini contains the user configurable settings for Wii Remote
        // extensions in addition to all of its lines that aren't user configurable, so since we
        // don't want to lose the selected extensions, we don't overwrite that file if it exists.
        //
        // TODO: Redo the Android controller system so that we don't have to extract these INIs.
        String configDirectory = NativeLibrary.GetUserDirectory() + File.separator + "Config";
        copyAsset("GCPadNew.ini", new File(configDirectory, "GCPadNew.ini"), true);
        copyAsset("WiimoteNew.ini", new File(configDirectory,"WiimoteNew.ini"), false);
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

    private void copyAsset(String asset, File output, Boolean overwrite)
    {
        Log.verbose("[DirectoryInitializationService] Copying File " + asset + " to " + output);

        try
        {
            if (!output.exists() || overwrite)
            {
                InputStream in = getAssets().open(asset);
                OutputStream out = new FileOutputStream(output);
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

    private void copyAssetFolder(String assetFolder, File outputFolder, Boolean overwrite)
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
                copyAssetFolder(assetFolder + File.separator + file, new File(outputFolder, file), overwrite);
                copyAsset(assetFolder + File.separator + file, new File(outputFolder, file), overwrite);
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
