/*
 * Copyright 2014 Dolphin Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Environment;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * A service that spawns its own thread in order to copy several binary and shader files
 * from the Dolphin APK to the external file system.
 */
public final class DirectoryInitialization
{
  public static final String EXTRA_STATE = "directoryState";
  private static final int WiimoteNewVersion = 5;  // Last changed in PR 8907
  private static final MutableLiveData<DirectoryInitializationState> directoryState =
          new MutableLiveData<>(DirectoryInitializationState.NOT_YET_INITIALIZED);
  private static volatile boolean areDirectoriesAvailable = false;
  private static String userPath;
  private static boolean isUsingLegacyUserDirectory = false;

  public enum DirectoryInitializationState
  {
    NOT_YET_INITIALIZED,
    INITIALIZING,
    DOLPHIN_DIRECTORIES_INITIALIZED
  }

  public static void start(Context context)
  {
    if (directoryState.getValue() != DirectoryInitializationState.NOT_YET_INITIALIZED)
      return;

    directoryState.setValue(DirectoryInitializationState.INITIALIZING);

    // Can take a few seconds to run, so don't block UI thread.
    new Thread(() -> init(context)).start();
  }

  private static void init(Context context)
  {
    if (directoryState.getValue() == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
      return;

    if (!setDolphinUserDirectory(context))
    {
      Toast.makeText(context, R.string.external_storage_not_mounted, Toast.LENGTH_LONG).show();
      System.exit(1);
    }

    initializeInternalStorage(context);
    boolean wiimoteIniWritten = initializeExternalStorage(context);
    NativeLibrary.Initialize();
    NativeLibrary.ReportStartToAnalytics();

    areDirectoriesAvailable = true;

    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    if (IntSetting.MAIN_INTERFACE_THEME.getIntGlobal() !=
            preferences.getInt(ThemeHelper.CURRENT_THEME, ThemeHelper.DEFAULT))
    {
      preferences.edit()
              .putInt(ThemeHelper.CURRENT_THEME, IntSetting.MAIN_INTERFACE_THEME.getIntGlobal())
              .apply();
    }

    if (IntSetting.MAIN_INTERFACE_THEME_MODE.getIntGlobal() !=
            preferences.getInt(ThemeHelper.CURRENT_THEME_MODE,
                    AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM))
    {
      preferences.edit()
              .putInt(ThemeHelper.CURRENT_THEME_MODE,
                      IntSetting.MAIN_INTERFACE_THEME_MODE.getIntGlobal())
              .apply();
    }

    if (wiimoteIniWritten)
    {
      // This has to be done after calling NativeLibrary.Initialize(),
      // as it relies on the config system
      EmulationActivity.updateWiimoteNewIniPreferences(context);
    }

    directoryState.postValue(DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED);
  }

  @Nullable
  private static File getLegacyUserDirectoryPath()
  {
    File externalPath = Environment.getExternalStorageDirectory();
    if (externalPath == null)
      return null;

    return new File(externalPath, "dolphin-emu");
  }

  private static boolean setDolphinUserDirectory(Context context)
  {
    if (!Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState()))
      return false;

    isUsingLegacyUserDirectory =
            preferLegacyUserDirectory(context) && PermissionsHandler.hasWriteAccess(context);

    File path = isUsingLegacyUserDirectory ?
            getLegacyUserDirectoryPath() : context.getExternalFilesDir(null);

    if (path == null)
      return false;

    userPath = path.getAbsolutePath();

    Log.debug("[DirectoryInitialization] User Dir: " + userPath);
    NativeLibrary.SetUserDirectory(userPath);

    File cacheDir = context.getExternalCacheDir();
    if (cacheDir == null)
      return false;

    Log.debug("[DirectoryInitialization] Cache Dir: " + cacheDir.getPath());
    NativeLibrary.SetCacheDirectory(cacheDir.getPath());

    return true;
  }

  private static void initializeInternalStorage(Context context)
  {
    File sysDirectory = new File(context.getFilesDir(), "Sys");

    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    String revision = NativeLibrary.GetGitRevision();
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
    SetSysDirectory(sysDirectory.getPath());
  }

  // Returns whether the WiimoteNew.ini file was written to
  private static boolean initializeExternalStorage(Context context)
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
    String profileDirectory =
            NativeLibrary.GetUserDirectory() + File.separator + "Config/Profiles/Wiimote/";
    createWiimoteProfileDirectory(profileDirectory);

    copyAsset("GCPadNew.ini", new File(configDirectory, "GCPadNew.ini"), true, context);

    SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
    boolean overwriteWiimoteIni = prefs.getInt("WiimoteNewVersion", 0) != WiimoteNewVersion;
    boolean wiimoteIniWritten = copyAsset("WiimoteNew.ini",
            new File(configDirectory, "WiimoteNew.ini"), overwriteWiimoteIni, context);
    if (overwriteWiimoteIni)
    {
      SharedPreferences.Editor sPrefsEditor = prefs.edit();
      sPrefsEditor.putInt("WiimoteNewVersion", WiimoteNewVersion);
      sPrefsEditor.apply();
    }

    copyAsset("WiimoteProfile.ini", new File(profileDirectory, "WiimoteProfile.ini"), true,
            context);

    return wiimoteIniWritten;
  }

  private static void deleteDirectoryRecursively(@NonNull final File file)
  {
    if (file.isDirectory())
    {
      File[] files = file.listFiles();

      if (files == null)
      {
        return;
      }

      for (File child : files)
        deleteDirectoryRecursively(child);
    }
    if (!file.delete())
    {
      Log.error("[DirectoryInitialization] Failed to delete " + file.getAbsolutePath());
    }
  }

  public static boolean shouldStart(Context context)
  {
    return getDolphinDirectoriesState().getValue() ==
            DirectoryInitializationState.NOT_YET_INITIALIZED &&
            !isWaitingForWriteAccess(context);
  }

  public static boolean areDolphinDirectoriesReady()
  {
    return directoryState.getValue() ==
            DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED;
  }

  public static LiveData<DirectoryInitializationState> getDolphinDirectoriesState()
  {
    return directoryState;
  }

  public static String getUserDirectory()
  {
    if (!areDirectoriesAvailable)
    {
      throw new IllegalStateException(
              "DirectoryInitialization must run before accessing the user directory!");
    }
    return userPath;
  }

  public static File getGameListCache(Context context)
  {
    return new File(context.getExternalCacheDir(), "gamelist.cache");
  }

  private static boolean copyAsset(String asset, File output, Boolean overwrite, Context context)
  {
    Log.verbose("[DirectoryInitialization] Copying File " + asset + " to " + output);

    try
    {
      if (!output.exists() || overwrite)
      {
        try (InputStream in = context.getAssets().open(asset))
        {
          try (OutputStream out = new FileOutputStream(output))
          {
            copyFile(in, out);
            return true;
          }
        }
      }
    }
    catch (IOException e)
    {
      Log.error("[DirectoryInitialization] Failed to copy asset file: " + asset +
              e.getMessage());
    }
    return false;
  }

  private static void copyAssetFolder(String assetFolder, File outputFolder, Boolean overwrite,
          Context context)
  {
    Log.verbose("[DirectoryInitialization] Copying Folder " + assetFolder + " to " +
            outputFolder);

    try
    {
      String[] assetList = context.getAssets().list(assetFolder);

      if (assetList == null)
      {
        return;
      }

      boolean createdFolder = false;
      for (String file : assetList)
      {
        if (!createdFolder)
        {
          if (!outputFolder.mkdir())
          {
            Log.error("[DirectoryInitialization] Failed to create folder " +
                    outputFolder.getAbsolutePath());
          }
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

  private static void copyFile(InputStream in, OutputStream out) throws IOException
  {
    byte[] buffer = new byte[1024];
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
      if (!wiiPath.mkdirs())
      {
        Log.error("[DirectoryInitialization] Failed to create folder " + wiiPath.getAbsolutePath());
      }
    }
  }

  public static boolean preferOldFolderPicker(Context context)
  {
    // As of January 2021, ACTION_OPEN_DOCUMENT_TREE seems to be broken on the Nvidia Shield TV
    // (the activity can't be navigated correctly with a gamepad). We can use the old folder picker
    // for the time being - Android 11 hasn't been released for this device. We have an explicit
    // check for Android 11 below in hopes that Nvidia will fix this before releasing Android 11.
    //
    // No Android TV device other than the Nvidia Shield TV is known to have an implementation of
    // ACTION_OPEN_DOCUMENT or ACTION_OPEN_DOCUMENT_TREE that even launches, but "fortunately", no
    // Android TV device other than the Shield TV is known to be able to run Dolphin (either due to
    // the 64-bit requirement or due to the GLES 3.0 requirement), so we can ignore this problem.
    //
    // All phones which are running a compatible version of Android support ACTION_OPEN_DOCUMENT and
    // ACTION_OPEN_DOCUMENT_TREE, as this is required by the mobile Android CTS (unlike Android TV).

    return Build.VERSION.SDK_INT < Build.VERSION_CODES.R &&
            PermissionsHandler.isExternalStorageLegacy() && TvUtil.isLeanback(context);
  }

  private static boolean isExternalFilesDirEmpty(Context context)
  {
    File dir = context.getExternalFilesDir(null);
    if (dir == null)
      return false;  // External storage not available

    File[] contents = dir.listFiles();
    return contents == null || contents.length == 0;
  }

  private static boolean legacyUserDirectoryExists()
  {
    try
    {
      return getLegacyUserDirectoryPath().exists();
    }
    catch (SecurityException e)
    {
      // Most likely we don't have permission to read external storage.
      // Return true so that external storage permissions will be requested.
      //
      // Strangely, we don't seem to trigger this case in practice, even with no permissions...
      // But this only makes things more convenient for users, so no harm done.

      return true;
    }
  }

  private static boolean preferLegacyUserDirectory(Context context)
  {
    return PermissionsHandler.isExternalStorageLegacy() &&
            !PermissionsHandler.isWritePermissionDenied() &&
            isExternalFilesDirEmpty(context) && legacyUserDirectoryExists();
  }

  public static boolean isUsingLegacyUserDirectory()
  {
    return isUsingLegacyUserDirectory;
  }

  public static boolean isWaitingForWriteAccess(Context context)
  {
    // This first check is only for performance, not correctness
    if (directoryState.getValue() != DirectoryInitializationState.NOT_YET_INITIALIZED)
      return false;

    return preferLegacyUserDirectory(context) && !PermissionsHandler.hasWriteAccess(context);
  }

  private static native void CreateUserDirectories();

  private static native void SetSysDirectory(String path);
}
