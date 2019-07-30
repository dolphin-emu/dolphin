package org.dolphinemu.dolphinemu.features.settings.model;

import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

public class Settings
{
  public static final String SECTION_INI_CORE = "Core";
  public static final String SECTION_INI_DSP = "DSP";
  public static final String SECTION_INI_INTERFACE = "Interface";

  public static final String SECTION_GFX_SETTINGS = "Settings";
  public static final String SECTION_GFX_ENHANCEMENTS = "Enhancements";
  public static final String SECTION_GFX_HACKS = "Hacks";

  public static final String SECTION_DEBUG = "Debug";

  public static final String SECTION_WIIMOTE = "Wiimote";

  public static final String SECTION_BINDINGS = "Android";
  public static final String SECTION_CONTROLS = "Controls";
  public static final String SECTION_PROFILE = "Profile";

  public static final String SECTION_WII_IPL = "IPL";

  private static final Map<String, List<String>> configFileSectionsMap = new HashMap<>();

  static
  {
    configFileSectionsMap.put(SettingsFile.FILE_NAME_DOLPHIN,
      Arrays.asList(SECTION_INI_CORE, SECTION_INI_DSP, SECTION_INI_INTERFACE, SECTION_BINDINGS, SECTION_DEBUG, SECTION_WII_IPL));
    configFileSectionsMap.put(SettingsFile.FILE_NAME_GFX,
      Arrays.asList(SECTION_GFX_SETTINGS, SECTION_GFX_ENHANCEMENTS, SECTION_GFX_HACKS));
    configFileSectionsMap.put(SettingsFile.FILE_NAME_WIIMOTE,
      Arrays.asList(SECTION_WIIMOTE + 1, SECTION_WIIMOTE + 2, SECTION_WIIMOTE + 3, SECTION_WIIMOTE + 4));
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

  public void loadSettings(@Nullable String gameId)
  {
    this.sections = new Settings.SettingsSectionMap();

    HashSet<String> filesToExclude = new HashSet<>();
    if (!TextUtils.isEmpty(gameId))
    {
      // for per-game settings, don't load the WiiMoteNew.ini settings
      filesToExclude.add(SettingsFile.FILE_NAME_WIIMOTE);
    }

    loadDolphinSettings(filesToExclude);

    if (!TextUtils.isEmpty(gameId))
    {
      loadGenericGameSettings(gameId);
      loadCustomGameSettings(gameId);
    }
  }

  private void loadDolphinSettings(HashSet<String> filesToExclude)
  {
    for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet())
    {
      String fileName = entry.getKey();
      if (!filesToExclude.contains(fileName))
      {
        sections.putAll(SettingsFile.readFile(fileName));
      }
    }
  }

  private void loadGenericGameSettings(String gameId)
  {
    // generic game settings
    mergeSections(SettingsFile.readGenericGameSettings(gameId));
    mergeSections(SettingsFile.readGenericGameSettingsForAllRegions(gameId));
  }

  private void loadCustomGameSettings(String gameId)
  {
    // custom game settings
    mergeSections(SettingsFile.readCustomGameSettings(gameId));
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

  public void saveSettings()
  {
    for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet())
    {
      String fileName = entry.getKey();
      List<String> sectionNames = entry.getValue();
      TreeMap<String, SettingSection> iniSections = new TreeMap<>();
      for (String section : sectionNames)
      {
        iniSections.put(section, sections.get(section));
      }

      SettingsFile.saveFile(fileName, iniSections);
    }
  }

  public void saveCustomGameSettings(String gameId)
  {
    SettingsFile.saveCustomGameSettings(gameId, sections);
  }
}
