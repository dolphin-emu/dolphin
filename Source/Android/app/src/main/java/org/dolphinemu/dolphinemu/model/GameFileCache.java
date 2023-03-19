// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model;

import androidx.annotation.Keep;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.utils.ContentHandler;
import org.dolphinemu.dolphinemu.utils.IniFile;

import java.io.File;
import java.util.LinkedHashSet;

public class GameFileCache
{
  @Keep
  private long mPointer;

  public GameFileCache()
  {
    mPointer = newGameFileCache();
  }

  private static native long newGameFileCache();

  @Override
  public native void finalize();

  public static void addGameFolder(String path)
  {
    File dolphinFile = SettingsFile.getSettingsFile(Settings.FILE_DOLPHIN);
    IniFile dolphinIni = new IniFile(dolphinFile);
    LinkedHashSet<String> pathSet = getPathSet(false);
    int totalISOPaths =
            dolphinIni.getInt(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATHS, 0);

    if (!pathSet.contains(path))
    {
      dolphinIni.setInt(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATHS,
              totalISOPaths + 1);
      dolphinIni.setString(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATH_BASE +
              totalISOPaths, path);
      dolphinIni.save(dolphinFile);
      NativeLibrary.ReloadConfig();
    }
  }

  private static LinkedHashSet<String> getPathSet(boolean removeNonExistentFolders)
  {
    File dolphinFile = SettingsFile.getSettingsFile(Settings.FILE_DOLPHIN);
    IniFile dolphinIni = new IniFile(dolphinFile);
    LinkedHashSet<String> pathSet = new LinkedHashSet<>();
    int totalISOPaths =
            dolphinIni.getInt(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATHS, 0);

    for (int i = 0; i < totalISOPaths; i++)
    {
      String path = dolphinIni.getString(Settings.SECTION_INI_GENERAL,
              SettingsFile.KEY_ISO_PATH_BASE + i, "");

      if (ContentHandler.isContentUri(path) ? ContentHandler.exists(path) : new File(path).exists())
      {
        pathSet.add(path);
      }
    }

    if (removeNonExistentFolders && totalISOPaths > pathSet.size())
    {
      int setIndex = 0;

      dolphinIni.setInt(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATHS,
              pathSet.size());

      // One or more folders have been removed.
      for (String entry : pathSet)
      {
        dolphinIni.setString(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATH_BASE +
                setIndex, entry);

        setIndex++;
      }

      // Delete known unnecessary keys. Ignore i values beyond totalISOPaths.
      for (int i = setIndex; i < totalISOPaths; i++)
      {
        dolphinIni.deleteKey(Settings.SECTION_INI_GENERAL, SettingsFile.KEY_ISO_PATH_BASE + i);
      }

      dolphinIni.save(dolphinFile);
      NativeLibrary.ReloadConfig();
    }

    return pathSet;
  }

  public static String[] getAllGamePaths()
  {
    boolean recursiveScan = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBoolean();

    LinkedHashSet<String> folderPathsSet = getPathSet(true);

    String[] folderPaths = folderPathsSet.toArray(new String[0]);

    return getAllGamePaths(folderPaths, recursiveScan);
  }

  public static native String[] getAllGamePaths(String[] folderPaths, boolean recursiveScan);

  public synchronized native int getSize();

  public synchronized native GameFile[] getAllGames();

  public synchronized native GameFile addOrGet(String gamePath);

  /**
   * Sets the list of games to cache.
   *
   * Games which are in the passed-in list but not in the cache are scanned and added to the cache,
   * and games which are in the cache but not in the passed-in list are removed from the cache.
   *
   * @return true if the cache was modified
   */
  public synchronized native boolean update(String[] gamePaths);

  /**
   * For each game that already is in the cache, scans the folder that contains the game
   * for additional metadata files (PNG/XML).
   *
   * @return true if the cache was modified
   */
  public synchronized native boolean updateAdditionalMetadata();

  public synchronized native boolean load();

  public synchronized native boolean save();
}
