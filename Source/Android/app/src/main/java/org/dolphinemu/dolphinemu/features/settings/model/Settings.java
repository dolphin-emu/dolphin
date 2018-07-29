package org.dolphinemu.dolphinemu.features.settings.model;

import android.text.TextUtils;

import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivityView;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

public class Settings
{
    public static final String SECTION_INI_CORE = "Core";
    public static final String SECTION_INI_INTERFACE = "Interface";

    public static final String SECTION_GFX_SETTINGS = "Settings";
    public static final String SECTION_GFX_ENHANCEMENTS = "Enhancements";
    public static final String SECTION_GFX_HACKS = "Hacks";

    public static final String SECTION_STEREOSCOPY = "Stereoscopy";

    public static final String SECTION_WIIMOTE = "Wiimote";

    public static final String SECTION_BINDINGS = "Android";

    private String gameId;

    private static final Map<String, List<String>> configFileSectionsMap = new HashMap<>();

    static
    {
        configFileSectionsMap.put(SettingsFile.FILE_NAME_DOLPHIN, Arrays.asList(SECTION_INI_CORE, SECTION_INI_INTERFACE, SECTION_BINDINGS));
        configFileSectionsMap.put(SettingsFile.FILE_NAME_GFX, Arrays.asList(SECTION_GFX_SETTINGS, SECTION_GFX_ENHANCEMENTS, SECTION_GFX_HACKS, SECTION_STEREOSCOPY));
        configFileSectionsMap.put(SettingsFile.FILE_NAME_WIIMOTE, Arrays.asList(SECTION_WIIMOTE + 1, SECTION_WIIMOTE + 2, SECTION_WIIMOTE + 3, SECTION_WIIMOTE + 4));
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
            for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet())
            {
                String fileName = entry.getKey();
                sections.putAll(SettingsFile.readFile(fileName, view));
            }
        }
        else
        {
            // custom game settings
            sections.putAll(SettingsFile.readCustomGameSettings(gameId, view));
        }
    }

    public void loadSettings(String gameId, SettingsActivityView view)
    {
        this.gameId = gameId;
        loadSettings(view);
    }

    public void saveSettings(SettingsActivityView view)
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
        }
        else
        {
            // custom game settings
            view.showToastMessage("Saved settings for " + gameId);
            SettingsFile.saveCustomGameSettings(gameId, sections);
        }

    }
}
