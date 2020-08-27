package org.dolphinemu.dolphinemu.features.settings.model;

import android.content.Context;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivityView;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

public class Settings
{
  public static final String SECTION_INI_ANDROID = "Android";
  public static final String SECTION_INI_GENERAL = "General";
  public static final String SECTION_INI_CORE = "Core";
  public static final String SECTION_INI_INTERFACE = "Interface";
  public static final String SECTION_INI_DSP = "DSP";

  public static final String SECTION_LOGGER_LOGS = "Logs";
  public static final String SECTION_LOGGER_OPTIONS = "Options";

  public static final String SECTION_GFX_SETTINGS = "Settings";
  public static final String SECTION_GFX_ENHANCEMENTS = "Enhancements";
  public static final String SECTION_GFX_HACKS = "Hacks";

  public static final String SECTION_DEBUG = "Debug";

  public static final String SECTION_STEREOSCOPY = "Stereoscopy";

  public static final String SECTION_WIIMOTE = "Wiimote";

  public static final String SECTION_BINDINGS = "Android";
  public static final String SECTION_CONTROLS = "Controls";
  public static final String SECTION_PROFILE = "Profile";

  private static final String DSP_HLE = "0";
  private static final String DSP_LLE_RECOMPILER = "1";
  private static final String DSP_LLE_INTERPRETER = "2";

  public static final String SECTION_ANALYTICS = "Analytics";

  private String gameId;

  private static final Map<String, List<String>> configFileSectionsMap = new HashMap<>();

  static
  {
    configFileSectionsMap.put(SettingsFile.FILE_NAME_DOLPHIN,
            Arrays
                    .asList(SECTION_INI_ANDROID, SECTION_INI_GENERAL, SECTION_INI_CORE,
                            SECTION_INI_INTERFACE,
                            SECTION_INI_DSP, SECTION_BINDINGS, SECTION_ANALYTICS, SECTION_DEBUG));
    configFileSectionsMap.put(SettingsFile.FILE_NAME_GFX,
            Arrays.asList(SECTION_GFX_SETTINGS, SECTION_GFX_ENHANCEMENTS, SECTION_GFX_HACKS,
                    SECTION_STEREOSCOPY));
    configFileSectionsMap.put(SettingsFile.FILE_NAME_LOGGER,
            Arrays.asList(SECTION_LOGGER_LOGS, SECTION_LOGGER_OPTIONS));
    configFileSectionsMap.put(SettingsFile.FILE_NAME_WIIMOTE,
            Arrays.asList(SECTION_WIIMOTE + 1, SECTION_WIIMOTE + 2, SECTION_WIIMOTE + 3,
                    SECTION_WIIMOTE + 4));
  }

  /**
   * A HashMap<String, SettingSection> that constructs a new SettingSection instead of returning null
   * when getting a key not already in the map
   */
  public static final class SettingsSectionMap extends HashMap<String, SettingSection>
  {
    @Override
    public SettingSection get(Object key)
    {
      if (!(key instanceof String))
      {
        return null;
      }

      String stringKey = (String) key;

      if (!super.containsKey(stringKey))
      {
        SettingSection section = new SettingSection(stringKey);
        super.put(stringKey, section);
        return section;
      }
      return super.get(key);
    }
  }

  private HashMap<String, SettingSection> sections = new Settings.SettingsSectionMap();

  public SettingSection getSection(String sectionName)
  {
    return sections.get(sectionName);
  }

  public boolean isEmpty()
  {
    return sections.isEmpty();
  }

  public HashMap<String, SettingSection> getSections()
  {
    return sections;
  }

  public void loadSettings(SettingsActivityView view)
  {
    sections = new Settings.SettingsSectionMap();

    if (TextUtils.isEmpty(gameId))
    {
      loadDolphinSettings(view);
    }
    else
    {
      loadCustomGameSettings(gameId, view);
    }
  }

  private void loadDolphinSettings(SettingsActivityView view)
  {
    for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet())
    {
      String fileName = entry.getKey();
      sections.putAll(SettingsFile.readFile(fileName, view));
    }
  }

  private void loadGenericGameSettings(String gameId, SettingsActivityView view)
  {
    // generic game settings
    mergeSections(SettingsFile.readGenericGameSettings(gameId, view));
    mergeSections(SettingsFile.readGenericGameSettingsForAllRegions(gameId, view));
  }

  private void loadCustomGameSettings(String gameId, SettingsActivityView view)
  {
    // custom game settings
    mergeSections(SettingsFile.readCustomGameSettings(gameId, view));
  }

  public void loadWiimoteProfile(String gameId, String padId)
  {
    mergeSections(SettingsFile.readWiimoteProfile(gameId, padId));
  }

  private void mergeSections(HashMap<String, SettingSection> updatedSections)
  {
    for (Map.Entry<String, SettingSection> entry : updatedSections.entrySet())
    {
      if (sections.containsKey(entry.getKey()))
      {
        SettingSection originalSection = sections.get(entry.getKey());
        SettingSection updatedSection = entry.getValue();
        originalSection.mergeSection(updatedSection);
      }
      else
      {
        sections.put(entry.getKey(), entry.getValue());
      }
    }
  }

  public void loadSettings(String gameId, SettingsActivityView view)
  {
    this.gameId = gameId;
    loadSettings(view);
  }

  public void saveSettings(SettingsActivityView view, Context context, Set<String> modifiedSettings)
  {
    if (TextUtils.isEmpty(gameId))
    {
      view.showToastMessage("Saved settings to INI files");

      for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet())
      {
        String fileName = entry.getKey();
        List<String> sectionNames = entry.getValue();
        TreeMap<String, SettingSection> iniSections = new TreeMap<>();
        for (String section : sectionNames)
        {
          iniSections.put(section, sections.get(section));
        }

        SettingsFile.saveFile(fileName, iniSections, view);
      }

      if (modifiedSettings.contains(SettingsFile.KEY_DSP_ENGINE))
      {
        switch (NativeLibrary
                .GetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini", Settings.SECTION_INI_ANDROID,
                        SettingsFile.KEY_DSP_ENGINE, DSP_HLE))
        {
          case DSP_HLE:
            NativeLibrary
                    .SetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini", Settings.SECTION_INI_CORE,
                            SettingsFile.KEY_DSP_HLE, "True");
            NativeLibrary
                    .SetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini", Settings.SECTION_INI_DSP,
                            SettingsFile.KEY_DSP_ENABLE_JIT, "True");
            break;

          case DSP_LLE_RECOMPILER:
            NativeLibrary
                    .SetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini", Settings.SECTION_INI_CORE,
                            SettingsFile.KEY_DSP_HLE, "False");
            NativeLibrary
                    .SetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini", Settings.SECTION_INI_DSP,
                            SettingsFile.KEY_DSP_ENABLE_JIT, "True");
            break;

          case DSP_LLE_INTERPRETER:
            NativeLibrary
                    .SetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini", Settings.SECTION_INI_CORE,
                            SettingsFile.KEY_DSP_HLE, "False");
            NativeLibrary
                    .SetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini", Settings.SECTION_INI_DSP,
                            SettingsFile.KEY_DSP_ENABLE_JIT, "False");
            break;
        }
      }

      // Notify the native code of the changes
      NativeLibrary.ReloadConfig();
      NativeLibrary.ReloadWiimoteConfig();
      NativeLibrary.ReloadLoggerConfig();

      if (modifiedSettings.contains(SettingsFile.KEY_RECURSIVE_ISO_PATHS))
      {
        // Refresh game library
        GameFileCacheService.startRescan(context);
      }

      modifiedSettings.clear();
    }
    else
    {
      // custom game settings
      view.showToastMessage("Saved settings for " + gameId);
      SettingsFile.saveCustomGameSettings(gameId, sections);
    }
  }

  public void clearSettings()
  {
    sections.clear();
  }

  public boolean gameIniContainsJunk()
  {
    // Older versions of Android Dolphin would copy the entire contents of most of the global INIs
    // into any game INI that got saved (with some of the sections renamed to match the game INI
    // section names). The problems with this are twofold:
    //
    // 1. The user game INIs will contain entries that Dolphin doesn't support reading from
    //    game INIs. This is annoying when editing game INIs manually but shouldn't really be
    //    a problem for those who only use the GUI.
    //
    // 2. Global settings will stick around in user game INIs. For instance, if someone wants to
    //    change the texture cache accuracy to safe for all games, they have to edit not only the
    //    global settings but also every single game INI they have created, since the old value of
    //    the texture cache accuracy setting has been copied into every user game INI.
    //
    // These problems are serious enough that we should detect and delete such INI files.
    // Problem 1 is easy to detect, but due to the nature of problem 2, it's unfortunately not
    // possible to know which lines were added intentionally by the user and which lines were added
    // unintentionally, which is why we have to delete the whole file in order to fix everything.

    if (TextUtils.isEmpty(gameId))
      return false;

    SettingSection interfaceSection = sections.get("Interface");
    return interfaceSection != null && interfaceSection.getSetting("ThemeName") != null;
  }
}
