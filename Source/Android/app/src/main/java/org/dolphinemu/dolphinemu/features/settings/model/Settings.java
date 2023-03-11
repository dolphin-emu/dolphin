// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import android.content.Context;
import android.text.TextUtils;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.input.model.MappingCommon;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;

import java.io.Closeable;

public class Settings implements Closeable
{
  public static final String FILE_DOLPHIN = "Dolphin";
  public static final String FILE_SYSCONF = "SYSCONF";
  public static final String FILE_GFX = "GFX";
  public static final String FILE_LOGGER = "Logger";
  public static final String FILE_WIIMOTE = "WiimoteNew";
  public static final String FILE_GAME_SETTINGS_ONLY = "GameSettingsOnly";

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
  public static final String SECTION_EMULATED_USB_DEVICES = "EmulatedUSBDevices";

  public static final String SECTION_STEREOSCOPY = "Stereoscopy";

  public static final String SECTION_BINDINGS = "Android";
  public static final String SECTION_PROFILE = "Profile";

  public static final String SECTION_ANALYTICS = "Analytics";

  private String mGameId;
  private int mRevision;
  private boolean mIsWii;

  private boolean mSettingsLoaded = false;

  private boolean mLoadedRecursiveIsoPathsValue = false;

  public boolean isGameSpecific()
  {
    return !TextUtils.isEmpty(mGameId);
  }

  public boolean isWii()
  {
    return mIsWii;
  }

  public int getWriteLayer()
  {
    return isGameSpecific() ? NativeConfig.LAYER_LOCAL_GAME : NativeConfig.LAYER_BASE_OR_CURRENT;
  }

  public boolean areSettingsLoaded()
  {
    return mSettingsLoaded;
  }

  public void loadSettings()
  {
    // The value of isWii doesn't matter if we don't have any SettingsActivity
    loadSettings(true);
  }

  public void loadSettings(boolean isWii)
  {
    mIsWii = isWii;
    mSettingsLoaded = true;

    if (isGameSpecific())
    {
      // Loading game INIs while the core is running will mess with the game INIs loaded by the core
      if (NativeLibrary.IsRunning())
        throw new IllegalStateException("Attempted to load game INI while emulating");

      NativeConfig.loadGameInis(mGameId, mRevision);
    }

    mLoadedRecursiveIsoPathsValue = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBoolean();
  }

  public void loadSettings(String gameId, int revision, boolean isWii)
  {
    mGameId = gameId;
    mRevision = revision;
    loadSettings(isWii);
  }

  public void saveSettings(Context context)
  {
    if (!isGameSpecific())
    {
      if (context != null)
        Toast.makeText(context, R.string.settings_saved, Toast.LENGTH_SHORT).show();

      MappingCommon.save();

      NativeConfig.save(NativeConfig.LAYER_BASE);

      NativeLibrary.ReloadLoggerConfig();
      NativeLibrary.UpdateGCAdapterScanThread();

      if (mLoadedRecursiveIsoPathsValue != BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.getBoolean())
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

      NativeConfig.save(NativeConfig.LAYER_LOCAL_GAME);
    }
  }

  public void clearGameSettings()
  {
    NativeConfig.deleteAllKeys(NativeConfig.LAYER_LOCAL_GAME);
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

    return NativeConfig.exists(NativeConfig.LAYER_LOCAL_GAME, FILE_DOLPHIN, SECTION_INI_INTERFACE,
            "ThemeName");
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
