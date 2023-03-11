// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.utils;

import androidx.annotation.NonNull;

import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivityView;
import org.dolphinemu.dolphinemu.utils.BiMap;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.IniFile;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.File;

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
public final class SettingsFile
{
  public static final String KEY_ISO_PATH_BASE = "ISOPath";
  public static final String KEY_ISO_PATHS = "ISOPaths";

  private static BiMap<String, String> sectionsMap = new BiMap<>();

  static
  {
    sectionsMap.add("Hardware", "Video_Hardware");
    sectionsMap.add("Settings", "Video_Settings");
    sectionsMap.add("Enhancements", "Video_Enhancements");
    sectionsMap.add("Stereoscopy", "Video_Stereoscopy");
    sectionsMap.add("Hacks", "Video_Hacks");
    sectionsMap.add("GameSpecific", "Video");
  }

  private SettingsFile()
  {
  }

  /**
   * Reads a given .ini file from disk and returns it.
   * If unsuccessful, outputs an error telling why it failed.
   *
   * @param file The ini file to load the settings from
   * @param ini  The object to load into
   * @param view The current view.
   */
  static void readFile(final File file, IniFile ini, SettingsActivityView view)
  {
    if (!ini.load(file, true))
    {
      Log.error("[SettingsFile] Error reading from: " + file.getAbsolutePath());
      if (view != null)
        view.onSettingsFileNotFound();
    }
  }

  public static void readFile(final String fileName, IniFile ini, SettingsActivityView view)
  {
    readFile(getSettingsFile(fileName), ini, view);
  }

  /**
   * Reads a given .ini file from disk and returns it.
   * If unsuccessful, outputs an error telling why it failed.
   *
   * @param gameId the id of the game to load settings for.
   * @param ini    The object to load into
   * @param view   The current view.
   */
  public static void readCustomGameSettings(final String gameId, IniFile ini,
          SettingsActivityView view)
  {
    readFile(getCustomGameSettingsFile(gameId), ini, view);
  }

  /**
   * Saves a given .ini file on disk.
   * If unsuccessful, outputs an error telling why it failed.
   *
   * @param fileName The target filename without a path or extension.
   * @param ini      The IniFile we want to serialize.
   * @param view     The current view.
   */
  public static void saveFile(final String fileName, IniFile ini, SettingsActivityView view)
  {
    if (!ini.save(getSettingsFile(fileName)))
    {
      Log.error("[SettingsFile] Error saving to: " + fileName + ".ini");
      if (view != null)
        view.showToastMessage("Error saving " + fileName + ".ini");
    }
  }

  public static void saveCustomGameSettings(final String gameId, IniFile ini)
  {
    ini.save(getCustomGameSettingsFile(gameId));
  }

  public static String mapSectionNameFromIni(String generalSectionName)
  {
    if (sectionsMap.getForward(generalSectionName) != null)
    {
      return sectionsMap.getForward(generalSectionName);
    }

    return generalSectionName;
  }

  public static String mapSectionNameToIni(String generalSectionName)
  {
    if (sectionsMap.getBackward(generalSectionName) != null)
    {
      return sectionsMap.getBackward(generalSectionName);
    }

    return generalSectionName;
  }

  @NonNull
  public static File getSettingsFile(String fileName)
  {
    return new File(
            DirectoryInitialization.getUserDirectory() + "/Config/" + fileName + ".ini");
  }

  public static File getCustomGameSettingsFile(String gameId)
  {
    return new File(
            DirectoryInitialization.getUserDirectory() + "/GameSettings/" + gameId + ".ini");
  }
}
