// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import android.content.Context;
import android.text.TextUtils;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivityView;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.IniFile;

import java.io.Closeable;
import java.util.HashMap;
import java.util.Map;

public class Settings implements Closeable
{
  public static final String FILE_DOLPHIN = "Dolphin";
  public static final String FILE_SYSCONF = "SYSCONF";
  public static final String FILE_GFX = "GFX";
  public static final String FILE_LOGGER = "Logger";
  public static final String FILE_GCPAD = "GCPadNew";
  public static final String FILE_WIIMOTE = "WiimoteNew";

  public static final String SECTION_INI_ANDROID = "Android";
  public static final String SECTION_INI_ANDROID_OVERLAY_BUTTONS = "AndroidOverlayButtons";
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

  public static final String SECTION_ANALYTICS = "Analytics";

  public static final String GAME_SETTINGS_PLACEHOLDER_FILE_NAME = "";

  private String mGameId;
  private int mRevision;
  private boolean mIsWii;

  private static final String[] configFiles = new String[]{FILE_DOLPHIN, FILE_GFX, FILE_LOGGER,
          FILE_WIIMOTE};

  private Map<String, IniFile> mIniFiles = new HashMap<>();
  private final Map<String, IniFile> mWiimoteProfileFiles = new HashMap<>();

  private boolean mLoadedRecursiveIsoPathsValue = false;

  private IniFile getGameSpecificFile()
  {
    if (!isGameSpecific() || mIniFiles.size() != 1)
      throw new IllegalStateException();

    return mIniFiles.get(GAME_SETTINGS_PLACEHOLDER_FILE_NAME);
  }

  public IniFile.Section getSection(String fileName, String sectionName)
  {
    if (!isGameSpecific())
    {
      return mIniFiles.get(fileName).getOrCreateSection(sectionName);
    }
    else
    {
      return getGameSpecificFile()
              .getOrCreateSection(SettingsFile.mapSectionNameFromIni(sectionName));
    }
  }

  public boolean isGameSpecific()
  {
    return !TextUtils.isEmpty(mGameId);
  }

  public boolean isWii()
  {
    return mIsWii;
  }

  public IniFile getWiimoteProfile(String profile, int padID)
  {
    IniFile wiimoteProfileIni = mWiimoteProfileFiles.computeIfAbsent(profile, profileComputed ->
    {
      IniFile newIni = new IniFile();
      newIni.load(SettingsFile.getWiiProfile(profileComputed), false);
      return newIni;
    });

    if (!wiimoteProfileIni.exists(SECTION_PROFILE))
    {
      String defaultWiiProfilePath = DirectoryInitialization.getUserDirectory() +
              "/Config/Profiles/Wiimote/WiimoteProfile.ini";

      wiimoteProfileIni.load(defaultWiiProfilePath, false);

      wiimoteProfileIni
              .setString(SECTION_PROFILE, "Device", "Android/" + (padID + 4) + "/Touchscreen");
    }

    return wiimoteProfileIni;
  }

  public void enableWiimoteProfile(Settings settings, String profile, String profileKey)
  {
    getWiimoteControlsSection(settings).setString(profileKey, profile);
  }

  public boolean disableWiimoteProfile(Settings settings, String profileKey)
  {
    return getWiimoteControlsSection(settings).delete(profileKey);
  }

  public boolean isWiimoteProfileEnabled(Settings settings, String profile,
          String profileKey)
  {
    return profile.equals(getWiimoteControlsSection(settings).getString(profileKey, ""));
  }

  private IniFile.Section getWiimoteControlsSection(Settings settings)
  {
    return settings.getSection(GAME_SETTINGS_PLACEHOLDER_FILE_NAME, SECTION_CONTROLS);
  }

  public int getWriteLayer()
  {
    return isGameSpecific() ? NativeConfig.LAYER_LOCAL_GAME : NativeConfig.LAYER_BASE_OR_CURRENT;
  }

  public boolean isEmpty()
  {
    return mIniFiles.isEmpty();
  }

  public void loadSettings()
  {
    // The value of isWii doesn't matter if we don't have any SettingsActivity
    loadSettings(null, true);
  }

  public void loadSettings(SettingsActivityView view, boolean isWii)
  {
    mIsWii = isWii;

    mIniFiles = new HashMap<>();

    if (!isGameSpecific())
    {
      loadDolphinSettings(view);
    }
    else
    {
      // Loading game INIs while the core is running will mess with the game INIs loaded by the core
      if (NativeLibrary.IsRunning())
        throw new IllegalStateException("Attempted to load game INI while emulating");

      NativeConfig.loadGameInis(mGameId, mRevision);
      loadCustomGameSettings(mGameId, view);
    }

    mLoadedRecursiveIsoPathsValue = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBoolean(this);
  }

  private void loadDolphinSettings(SettingsActivityView view)
  {
    for (String fileName : configFiles)
    {
      IniFile ini = new IniFile();
      SettingsFile.readFile(fileName, ini, view);
      mIniFiles.put(fileName, ini);
    }
  }

  private void loadCustomGameSettings(String gameId, SettingsActivityView view)
  {
    IniFile ini = new IniFile();
    SettingsFile.readCustomGameSettings(gameId, ini, view);
    mIniFiles.put(GAME_SETTINGS_PLACEHOLDER_FILE_NAME, ini);
  }

  public void loadSettings(SettingsActivityView view, String gameId, int revision, boolean isWii)
  {
    mGameId = gameId;
    mRevision = revision;
    loadSettings(view, isWii);
  }

  public void saveSettings(SettingsActivityView view, Context context)
  {
    if (!isGameSpecific())
    {
      if (context != null)
        Toast.makeText(context, R.string.settings_saved, Toast.LENGTH_SHORT).show();

      for (Map.Entry<String, IniFile> entry : mIniFiles.entrySet())
      {
        SettingsFile.saveFile(entry.getKey(), entry.getValue(), view);
      }

      NativeConfig.save(NativeConfig.LAYER_BASE);

      if (!NativeLibrary.IsRunning())
      {
        // Notify the native code of the changes to legacy settings
        NativeLibrary.ReloadConfig();
        NativeLibrary.ReloadWiimoteConfig();
      }

      // LogManager does use the new config system, but doesn't pick up on changes automatically
      NativeLibrary.ReloadLoggerConfig();
      NativeLibrary.UpdateGCAdapterScanThread();

      if (mLoadedRecursiveIsoPathsValue != BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBoolean(this))
      {
        // Refresh game library
        GameFileCacheManager.startRescan();
      }
    }
    else
    {
      // custom game settings

      if (context != null)
      {
        Toast.makeText(context, context.getString(R.string.settings_saved_game_specific, mGameId),
                Toast.LENGTH_SHORT).show();
      }

      SettingsFile.saveCustomGameSettings(mGameId, getGameSpecificFile());

      NativeConfig.save(NativeConfig.LAYER_LOCAL_GAME);
    }

    for (Map.Entry<String, IniFile> entry : mWiimoteProfileFiles.entrySet())
    {
      entry.getValue().save(SettingsFile.getWiiProfile(entry.getKey()));
    }
  }

  public void clearSettings()
  {
    for (String fileName : mIniFiles.keySet())
    {
      mIniFiles.put(fileName, new IniFile());
    }
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

    if (!isGameSpecific())
      return false;

    return getSection(Settings.FILE_DOLPHIN, SECTION_INI_INTERFACE).exists("ThemeName");
  }

  @Override
  public void close()
  {
    if (isGameSpecific())
    {
      NativeConfig.unloadGameInis();
    }
  }
}
