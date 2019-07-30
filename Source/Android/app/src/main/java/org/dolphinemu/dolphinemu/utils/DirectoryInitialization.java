/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.NativeLibrary;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;


/**
 * A service that spawns its own thread in order to copy several binary and shader files
 * from the Dolphin APK to the external file system.
 */
public final class DirectoryInitialization
{
  public static final String BROADCAST_ACTION =
    "org.dolphinemu.dolphinemu.DIRECTORY_INITIALIZATION";

  public static final String EXTRA_STATE = "directoryState";
  private static final Integer WiimoteNewVersion = 2;
  private static volatile DirectoryInitializationState mDirectoryState;
  private static String mUserPath;
  private static String mInternalPath;
  private static AtomicBoolean mIsRunning = new AtomicBoolean(false);

  public enum DirectoryInitializationState
  {
    DIRECTORIES_INITIALIZED,
    EXTERNAL_STORAGE_PERMISSION_NEEDED,
    CANT_FIND_EXTERNAL_STORAGE
  }

  public static void start(Context context)
  {
    // Can take a few seconds to run, so don't block UI thread.
    //noinspection TrivialFunctionalExpressionUsage
    ((Runnable) () -> init(context)).run();
  }

  private static void init(Context context)
  {
    if (!mIsRunning.compareAndSet(false, true))
      return;

    if (mDirectoryState != DirectoryInitializationState.DIRECTORIES_INITIALIZED)
    {
      if (PermissionsHandler.hasWriteAccess(context))
      {
        if (setDolphinUserDirectory())
        {
          initializeInternalStorage(context);
          initializeExternalStorage(context);
          String lan = Locale.getDefault().getLanguage();
          if(lan.equals("zh"))
            lan = lan + "_" + Locale.getDefault().getCountry();
          NativeLibrary.setSystemLanguage(lan);
          mDirectoryState = DirectoryInitializationState.DIRECTORIES_INITIALIZED;
        }
        else
        {
          mDirectoryState = DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE;
        }
      }
      else
      {
        mDirectoryState = DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED;
      }
    }

    mIsRunning.set(false);
    sendBroadcastState(mDirectoryState, context);
  }

  private static boolean setDolphinUserDirectory()
  {
    if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState()))
    {
      File externalPath = Environment.getExternalStorageDirectory();
      if (externalPath != null)
      {
        File userPath = new File(externalPath, "dolphin-mmj");
        if (!userPath.isDirectory() && !userPath.mkdir())
        {
          return false;
        }
        mUserPath = userPath.getPath();
        NativeLibrary.SetUserDirectory(mUserPath);
        return true;
      }
    }
    return false;
  }

  private static void initializeInternalStorage(Context context)
  {
    File sysDirectory = new File(context.getFilesDir(), "Sys");
    mInternalPath = sysDirectory.getAbsolutePath();

    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    String revision = String.valueOf(BuildConfig.VERSION_CODE);
    if (!preferences.getString("sysDirectoryVersion", "").equals(revision))
    {
      // There is no extracted Sys directory, or there is a Sys directory from another
      // version of Dolphin that might contain outdated files. Let's (re-)extract Sys.
      deleteDirectoryRecursively(sysDirectory);
      copyAssetFolder("Sys", sysDirectory, true, context);

      SharedPreferences.Editor editor = preferences.edit();
      editor.putString("sysDirectoryVersion", revision);
      editor.apply();
    }

    // Let the native code know where the Sys directory is.
    NativeLibrary.SetSysDirectory(sysDirectory.getPath());
  }

  private static void initializeExternalStorage(Context context)
  {
    // Create User directory structure and copy some NAND files from the extracted Sys directory.
    NativeLibrary.CreateUserDirectories();

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
    String profileDirectory =
            NativeLibrary.GetUserDirectory() + File.separator + "Config/Profiles/Wiimote/";
    createWiimoteProfileDirectory(profileDirectory);

    copyAsset("GCPadNew.ini", new File(configDirectory, "GCPadNew.ini"), true, context);

    SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
    if (prefs.getInt("WiimoteNewVersion", 0) != WiimoteNewVersion)
    {
      copyAsset("WiimoteNew.ini", new File(configDirectory, "WiimoteNew.ini"), true, context);
      SharedPreferences.Editor sPrefsEditor = prefs.edit();
      sPrefsEditor.putInt("WiimoteNewVersion", WiimoteNewVersion);
      sPrefsEditor.apply();
    }
    else
    {
      copyAsset("WiimoteNew.ini", new File(configDirectory, "WiimoteNew.ini"), false, context);
    }

    copyAsset("WiimoteProfile.ini", new File(profileDirectory, "WiimoteProfile.ini"), true,
            context);
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

  public static boolean isReady()
  {
    return mDirectoryState == DirectoryInitializationState.DIRECTORIES_INITIALIZED;
  }

  public static String getUserDirectory()
  {
    if (mDirectoryState == null)
    {
      throw new IllegalStateException("DirectoryInitialization has to run at least once!");
    }
    else if (mIsRunning.get())
    {
      throw new IllegalStateException("DirectoryInitialization has to finish running first!");
    }
    return mUserPath;
  }

  public static String getCacheDirectory()
  {
    return getUserDirectory() + File.separator + "Cache";
  }

  public static String getCoverDirectory()
  {
    return getCacheDirectory() + File.separator + "GameCovers";
  }

  public static String getLocalSettingFile(String gameId)
  {
    return getUserDirectory() + File.separator + "GameSettings" + File.separator + gameId +
      ".ini";
  }

  public static String getInternalDirectory()
  {
    if (mDirectoryState == null)
    {
      throw new IllegalStateException("DirectoryInitialization has to run at least once!");
    }
    else if (mIsRunning.get())
    {
      throw new IllegalStateException("DirectoryInitialization has to finish running first!");
    }
    return mInternalPath;
  }

  private static void sendBroadcastState(DirectoryInitializationState state, Context context)
  {
    Intent localIntent = new Intent(BROADCAST_ACTION).putExtra(EXTRA_STATE, state);
    LocalBroadcastManager.getInstance(context).sendBroadcast(localIntent);
  }

  private static void copyAsset(String asset, File output, Boolean overwrite, Context context)
  {
    Log.verbose("[DirectoryInitialization] Copying File " + asset + " to " + output);

    try
    {
      if (!output.exists() || overwrite)
      {
        InputStream in = context.getAssets().open(asset);
        OutputStream out = new FileOutputStream(output);
        copyFile(in, out);
        in.close();
        out.close();
      }
    }
    catch (IOException e)
    {
      Log.error("[DirectoryInitialization] Failed to copy asset file: " + asset +
        e.getMessage());
    }
  }

  private static void copyAssetFolder(String assetFolder, File outputFolder, Boolean overwrite,
    Context context)
  {
    Log.verbose("[DirectoryInitialization] Copying Folder " + assetFolder + " to " + outputFolder);

    try
    {
      boolean createdFolder = false;
      for (String file : context.getAssets().list(assetFolder))
      {
        if (!createdFolder)
        {
          outputFolder.mkdir();
          createdFolder = true;
        }
        copyAssetFolder(assetFolder + File.separator + file, new File(outputFolder, file),
          overwrite, context);
        copyAsset(assetFolder + File.separator + file, new File(outputFolder, file), overwrite,
          context);
      }
    }
    catch (IOException e)
    {
      Log.error("[DirectoryInitialization] Failed to copy asset folder: " + assetFolder +
        e.getMessage());
    }
  }

  public static void copyFile(String from, String to)
  {
    try
    {
      InputStream in = new FileInputStream(from);
      OutputStream out = new FileOutputStream(to);
      copyFile(in, out);
    }
    catch (IOException e)
    {

    }
  }

  private static void copyFile(InputStream in, OutputStream out) throws IOException
  {
    byte[] buffer = new byte[4096];
    int read;

    while ((read = in.read(buffer)) != -1)
    {
      out.write(buffer, 0, read);
    }
  }

  private static void createWiimoteProfileDirectory(String directory)
  {
    File wiiPath = new File(directory);
    if (!wiiPath.isDirectory())
    {
      wiiPath.mkdirs();
    }
  }
}
