package org.dolphinemu.dolphinemu.model;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

import java.io.File;
import java.util.HashSet;
import java.util.Set;

public class GameFileCache
{
  private static final String GAME_FOLDER_PATHS_PREFERENCE = "gameFolderPaths";
  private static final Set<String> EMPTY_SET = new HashSet<>();

  private long mPointer;  // Do not rename or move without editing the native code

  public GameFileCache(String path)
  {
    mPointer = newGameFileCache(path);
  }

  private static native long newGameFileCache(String path);

  @Override
  public native void finalize();

  public static void addGameFolder(String path, Context context)
  {
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    Set<String> folderPaths = preferences.getStringSet(GAME_FOLDER_PATHS_PREFERENCE, EMPTY_SET);
    Set<String> newFolderPaths = new HashSet<>(folderPaths);
    newFolderPaths.add(path);
    SharedPreferences.Editor editor = preferences.edit();
    editor.putStringSet(GAME_FOLDER_PATHS_PREFERENCE, newFolderPaths);
    editor.apply();
  }

  private void removeNonExistentGameFolders(Context context)
  {
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    Set<String> folderPaths = preferences.getStringSet(GAME_FOLDER_PATHS_PREFERENCE, EMPTY_SET);
    Set<String> newFolderPaths = new HashSet<>();
    for (String folderPath : folderPaths)
    {
      File folder = new File(folderPath);
      if (folder.exists())
      {
        newFolderPaths.add(folderPath);
      }
    }

    if (folderPaths.size() != newFolderPaths.size())
    {
      // One or more folders are being deleted
      SharedPreferences.Editor editor = preferences.edit();
      editor.putStringSet(GAME_FOLDER_PATHS_PREFERENCE, newFolderPaths);
      editor.apply();
    }
  }

  /**
   * Scans through the file system and updates the cache to match.
   *
   * @return true if the cache was modified
   */
  public boolean scanLibrary(Context context)
  {
    boolean recursiveScan = NativeLibrary
            .GetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini", Settings.SECTION_INI_GENERAL,
                    SettingsFile.KEY_RECURSIVE_ISO_PATHS, "False").equals("True");

    removeNonExistentGameFolders(context);

    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    Set<String> folderPathsSet = preferences.getStringSet(GAME_FOLDER_PATHS_PREFERENCE, EMPTY_SET);
    String[] folderPaths = folderPathsSet.toArray(new String[folderPathsSet.size()]);

    boolean cacheChanged = update(folderPaths, recursiveScan);
    cacheChanged |= updateAdditionalMetadata();
    if (cacheChanged)
    {
      save();
    }
    return cacheChanged;
  }

  public native GameFile[] getAllGames();

  public native GameFile addOrGet(String gamePath);

  private native boolean update(String[] folderPaths, boolean recursiveScan);

  private native boolean updateAdditionalMetadata();

  public native boolean load();

  private native boolean save();
}
