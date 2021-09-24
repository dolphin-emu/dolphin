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

      if (path.startsWith("content://") ? ContentHandler.exists(path) : new File(path).exists())
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

  /**
   * Scans through the file system and updates the cache to match.
   *
   * @return true if the cache was modified
   */
  public boolean update()
  {
    boolean recursiveScan = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBooleanGlobal();

    LinkedHashSet<String> folderPathsSet = getPathSet(true);

    String[] folderPaths = folderPathsSet.toArray(new String[0]);

    return update(folderPaths, recursiveScan);
  }

  public native int getSize();

  public native GameFile[] getAllGames();

  public native GameFile addOrGet(String gamePath);

  public native boolean update(String[] folderPaths, boolean recursiveScan);

  public native boolean updateAdditionalMetadata();

  public native boolean load();

  public native boolean save();
}
