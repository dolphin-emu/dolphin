package org.dolphinemu.dolphinemu.features.settings.model;

import android.text.TextUtils;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivityView;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

public class Settings {
	public static final String SECTION_INI_CORE = "Core";
	public static final String SECTION_INI_DSP = "DSP";
	public static final String SECTION_INI_INTERFACE = "Interface";

	public static final String SECTION_GFX_SETTINGS = "Settings";
	public static final String SECTION_GFX_ENHANCEMENTS = "Enhancements";
	public static final String SECTION_GFX_HACKS = "Hacks";

	public static final String SECTION_WIIMOTE = "Wiimote";

	public static final String SECTION_BINDINGS = "Android";

	public static final String SECTION_SYSCONF = "Wii";

	private String gameId;

	private static final Map<String, List<String>> configFileSectionsMap = new HashMap<>();

	static {
		configFileSectionsMap.put(SettingsFile.FILE_NAME_DOLPHIN, Arrays.asList(SECTION_INI_CORE, SECTION_INI_DSP, SECTION_INI_INTERFACE, SECTION_BINDINGS));
		configFileSectionsMap.put(SettingsFile.FILE_NAME_GFX, Arrays.asList(SECTION_GFX_SETTINGS, SECTION_GFX_ENHANCEMENTS, SECTION_GFX_HACKS));
		configFileSectionsMap.put(SettingsFile.FILE_NAME_WIIMOTE, Arrays.asList(SECTION_WIIMOTE + 1, SECTION_WIIMOTE + 2, SECTION_WIIMOTE + 3, SECTION_WIIMOTE + 4));
	}

	/**
	 * A HashMap<String, SettingSection> that constructs a new SettingSection instead of returning null
	 * when getting a key not already in the map
	 */
	public static final class SettingsSectionMap extends HashMap<String, SettingSection> {
		@Override
		public SettingSection get(Object key) {
			if (!(key instanceof String)) {
				return null;
			}

			String stringKey = (String) key;

			if (!super.containsKey(stringKey)) {
				SettingSection section = new SettingSection(stringKey);
				super.put(stringKey, section);
				return section;
			}
			return super.get(key);
		}
	}

	private HashMap<String, SettingSection> sections = new Settings.SettingsSectionMap();

	public SettingSection getSection(String sectionName) {
		return sections.get(sectionName);
	}

	public boolean isEmpty() {
		return sections.isEmpty();
	}

	public HashMap<String, SettingSection> getSections() {
		return sections;
	}

	public void loadSettings(SettingsActivityView view) {
		sections = new Settings.SettingsSectionMap();

		HashSet<String> filesToExclude = new HashSet<>();
		if (!TextUtils.isEmpty(gameId)) {
			// for per-game settings, don't load the WiiMoteNew.ini settings
			filesToExclude.add(SettingsFile.FILE_NAME_WIIMOTE);
		}

		loadDolphinSettings(view, filesToExclude);

		if (!TextUtils.isEmpty(gameId)) {
			loadGenericGameSettings(gameId, view);
			loadCustomGameSettings(gameId, view);
		}

		loadWiiSysconf();
	}

	private void loadDolphinSettings(SettingsActivityView view, HashSet<String> filesToExclude) {
		for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet()) {
			String fileName = entry.getKey();
			if (filesToExclude == null || !filesToExclude.contains(fileName)) {
				sections.putAll(SettingsFile.readFile(fileName, view));
			}
		}
	}

	private void loadWiiSysconf() {
		int[] sysconf = NativeLibrary.getSysconfSettings();
		SettingSection section = new SettingSection(SECTION_SYSCONF);

		Setting setting = new BooleanSetting(SettingsFile.KEY_SYSCONF_SCREENSAVER, SECTION_SYSCONF,
			sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_SCREENSAVER] != 0);
		section.putSetting(setting);

		setting = new IntSetting(SettingsFile.KEY_SYSCONF_LANGUAGE, SECTION_SYSCONF,
			sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_LANGUAGE]);
		section.putSetting(setting);

		setting = new BooleanSetting(SettingsFile.KEY_SYSCONF_WIDESCREEN, SECTION_SYSCONF,
			sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_WIDESCREEN] != 0);
		section.putSetting(setting);

		setting = new BooleanSetting(SettingsFile.KEY_SYSCONF_PROGRESSIVE_SCAN, SECTION_SYSCONF,
			sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_PROGRESSIVE_SCAN] != 0);
		section.putSetting(setting);

		setting = new BooleanSetting(SettingsFile.KEY_SYSCONF_PAL60, SECTION_SYSCONF,
			sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_PAL60] != 0);
		section.putSetting(setting);

		setting = new IntSetting(SettingsFile.KEY_SYSCONF_SENSOR_BAR_POSITION, SECTION_SYSCONF,
			sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_SENSOR_BAR_POSITION]);
		section.putSetting(setting);

		int sensitivity = sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_SENSOR_BAR_SENSITIVITY];
		setting = new IntSetting(SettingsFile.KEY_SYSCONF_SENSOR_BAR_SENSITIVITY, SECTION_SYSCONF,
			(int)((sensitivity - 4) / 123.0f * 100.0f));
		section.putSetting(setting);

		float voulme = sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_SPEAKER_VOLUME];
		setting = new IntSetting(SettingsFile.KEY_SYSCONF_SPEAKER_VOLUME, SECTION_SYSCONF,
			(int)(voulme / 127.0f * 100.0f));
		section.putSetting(setting);

		setting = new BooleanSetting(SettingsFile.KEY_SYSCONF_WIIMOTE_MOTOR, SECTION_SYSCONF,
			sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_WIIMOTE_MOTOR] != 0);
		section.putSetting(setting);

		sections.put(SECTION_SYSCONF, section);
	}

	private void saveWiiSysconf() {
		int[] sysconf = new int[9];
		SettingSection section =sections.get(SECTION_SYSCONF);

		BooleanSetting boolSetting = (BooleanSetting)section.getSetting(SettingsFile.KEY_SYSCONF_SCREENSAVER);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_SCREENSAVER] = boolSetting.getValue() ? 1 : 0;

		IntSetting intSetting = (IntSetting)section.getSetting(SettingsFile.KEY_SYSCONF_LANGUAGE);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_LANGUAGE] = intSetting.getValue();

		boolSetting = (BooleanSetting)section.getSetting(SettingsFile.KEY_SYSCONF_WIDESCREEN);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_WIDESCREEN] = boolSetting.getValue() ? 1 : 0;

		boolSetting = (BooleanSetting)section.getSetting(SettingsFile.KEY_SYSCONF_PROGRESSIVE_SCAN);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_PROGRESSIVE_SCAN] = boolSetting.getValue() ? 1 : 0;

		boolSetting = (BooleanSetting)section.getSetting(SettingsFile.KEY_SYSCONF_PAL60);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_PAL60] = boolSetting.getValue() ? 1 : 0;

		intSetting = (IntSetting)section.getSetting(SettingsFile.KEY_SYSCONF_SENSOR_BAR_POSITION);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_SENSOR_BAR_POSITION] = intSetting.getValue();

		intSetting = (IntSetting)section.getSetting(SettingsFile.KEY_SYSCONF_SENSOR_BAR_SENSITIVITY);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_SENSOR_BAR_SENSITIVITY] = (int)(intSetting.getValue() / 100.0f * 123.0f + 4);

		intSetting = (IntSetting)section.getSetting(SettingsFile.KEY_SYSCONF_SPEAKER_VOLUME);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_SPEAKER_VOLUME] = (int)(intSetting.getValue() / 100.0f * 127.0f);

		boolSetting = (BooleanSetting)section.getSetting(SettingsFile.KEY_SYSCONF_WIIMOTE_MOTOR);
		sysconf[NativeLibrary.SYSCONFSetting.SYSCONF_WIIMOTE_MOTOR] = boolSetting.getValue() ? 1 : 0;

		NativeLibrary.setSysconfSettings(sysconf);
	}

	private void loadGenericGameSettings(String gameId, SettingsActivityView view) {
		// generic game settings
		mergeSections(SettingsFile.readGenericGameSettings(gameId, view));
		mergeSections(SettingsFile.readGenericGameSettingsForAllRegions(gameId, view));
	}

	private void loadCustomGameSettings(String gameId, SettingsActivityView view) {
		// custom game settings
		mergeSections(SettingsFile.readCustomGameSettings(gameId, view));
	}

	private void mergeSections(HashMap<String, SettingSection> updatedSections) {
		for (Map.Entry<String, SettingSection> entry : updatedSections.entrySet()) {
			if (sections.containsKey(entry.getKey())) {
				SettingSection originalSection = sections.get(entry.getKey());
				SettingSection updatedSection = entry.getValue();
				originalSection.mergeSection(updatedSection);
			} else {
				sections.put(entry.getKey(), entry.getValue());
			}
		}
	}

	public void loadSettings(String gameId, SettingsActivityView view) {
		this.gameId = gameId;
		loadSettings(view);
	}

	public void saveSettings(SettingsActivityView view) {
		if (TextUtils.isEmpty(gameId)) {
			view.showToastMessage("Saved settings to INI files");

			for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet()) {
				String fileName = entry.getKey();
				List<String> sectionNames = entry.getValue();
				TreeMap<String, SettingSection> iniSections = new TreeMap<>();
				for (String section : sectionNames) {
					iniSections.put(section, sections.get(section));
				}

				SettingsFile.saveFile(fileName, iniSections, view);
			}

			saveWiiSysconf();
		} else {
			// custom game settings
			view.showToastMessage("Saved settings for " + gameId);
			SettingsFile.saveCustomGameSettings(gameId, sections);
		}

	}
}
