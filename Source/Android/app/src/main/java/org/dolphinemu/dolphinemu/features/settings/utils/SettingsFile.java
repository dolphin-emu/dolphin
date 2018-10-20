package org.dolphinemu.dolphinemu.features.settings.utils;

import android.support.annotation.NonNull;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.FloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.SettingSection;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivityView;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.BiMap;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import java.util.HashMap;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
public final class SettingsFile
{
  public static final String FILE_NAME_DOLPHIN = "Dolphin";
  public static final String FILE_NAME_GFX = "GFX";
  public static final String FILE_NAME_GCPAD = "GCPadNew";
  public static final String FILE_NAME_WIIMOTE = "WiimoteNew";


  public static final String KEY_CPU_CORE = "CPUCore";
  public static final String KEY_DUAL_CORE = "CPUThread";
  public static final String KEY_OVERCLOCK_ENABLE = "OverclockEnable";
  public static final String KEY_OVERCLOCK_PERCENT = "Overclock";
  public static final String KEY_SPEED_LIMIT = "EmulationSpeed";
  public static final String KEY_VIDEO_BACKEND = "GFXBackend";
  public static final String KEY_AUDIO_STRETCH = "AudioStretch";
  public static final String KEY_GAME_CUBE_LANGUAGE = "SelectedLanguage";
  public static final String KEY_OVERRIDE_GAME_CUBE_LANGUAGE = "OverrideGCLang";
  public static final String KEY_SLOT_A_DEVICE = "SlotA";
  public static final String KEY_SLOT_B_DEVICE = "SlotB";
  public static final String KEY_ENABLE_SAVE_STATES = "EnableSaveStates";
  public static final String KEY_LOCK_LANDSCAPE = "LockLandscape";

  public static final String KEY_ANALYTICS_ENABLED = "Enabled";
  public static final String KEY_ANALYTICS_PERMISSION_ASKED = "PermissionAsked";

  public static final String KEY_USE_PANIC_HANDLERS = "UsePanicHandlers";
  public static final String KEY_OSD_MESSAGES = "OnScreenDisplayMessages";

  public static final String KEY_SHOW_FPS = "ShowFPS";
  public static final String KEY_INTERNAL_RES = "InternalResolution";
  public static final String KEY_FSAA = "MSAA";
  public static final String KEY_ANISOTROPY = "MaxAnisotropy";
  public static final String KEY_POST_SHADER = "PostProcessingShader";
  public static final String KEY_SCALED_EFB = "EFBScaledCopy";
  public static final String KEY_PER_PIXEL = "EnablePixelLighting";
  public static final String KEY_FORCE_FILTERING = "ForceFiltering";
  public static final String KEY_DISABLE_FOG = "DisableFog";
  public static final String KEY_DISABLE_COPY_FILTER = "DisableCopyFilter";
  public static final String KEY_ARBITRARY_MIPMAP_DETECTION = "ArbitraryMipmapDetection";
  public static final String KEY_WIDE_SCREEN_HACK = "wideScreenHack";
  public static final String KEY_FORCE_24_BIT_COLOR = "ForceTrueColor";

  public static final String KEY_STEREO_MODE = "StereoMode";
  public static final String KEY_STEREO_DEPTH = "StereoDepth";
  public static final String KEY_STEREO_CONV = "StereoConvergencePercentage";
  public static final String KEY_STEREO_SWAP = "StereoSwapEyes";

  public static final String KEY_SKIP_EFB = "EFBAccessEnable";
  public static final String KEY_IGNORE_FORMAT = "EFBEmulateFormatChanges";
  public static final String KEY_EFB_TEXTURE = "EFBToTextureEnable";
  public static final String KEY_TEXCACHE_ACCURACY = "SafeTextureCacheColorSamples";
  public static final String KEY_GPU_TEXTURE_DECODING = "EnableGPUTextureDecoding";
  public static final String KEY_XFB_TEXTURE = "XFBToTextureEnable";
  public static final String KEY_IMMEDIATE_XFB = "ImmediateXFBEnable";
  public static final String KEY_FAST_DEPTH = "FastDepthCalc";
  public static final String KEY_ASPECT_RATIO = "AspectRatio";
  public static final String KEY_SHADER_COMPILATION_MODE = "ShaderCompilationMode";
  public static final String KEY_WAIT_FOR_SHADERS = "WaitForShadersBeforeStarting";

  public static final String KEY_GCPAD_TYPE = "SIDevice";

  public static final String KEY_GCBIND_A = "InputA_";
  public static final String KEY_GCBIND_B = "InputB_";
  public static final String KEY_GCBIND_X = "InputX_";
  public static final String KEY_GCBIND_Y = "InputY_";
  public static final String KEY_GCBIND_Z = "InputZ_";
  public static final String KEY_GCBIND_START = "InputStart_";
  public static final String KEY_GCBIND_CONTROL_UP = "MainUp_";
  public static final String KEY_GCBIND_CONTROL_DOWN = "MainDown_";
  public static final String KEY_GCBIND_CONTROL_LEFT = "MainLeft_";
  public static final String KEY_GCBIND_CONTROL_RIGHT = "MainRight_";
  public static final String KEY_GCBIND_C_UP = "CStickUp_";
  public static final String KEY_GCBIND_C_DOWN = "CStickDown_";
  public static final String KEY_GCBIND_C_LEFT = "CStickLeft_";
  public static final String KEY_GCBIND_C_RIGHT = "CStickRight_";
  public static final String KEY_GCBIND_TRIGGER_L = "InputL_";
  public static final String KEY_GCBIND_TRIGGER_R = "InputR_";
  public static final String KEY_GCBIND_DPAD_UP = "DPadUp_";
  public static final String KEY_GCBIND_DPAD_DOWN = "DPadDown_";
  public static final String KEY_GCBIND_DPAD_LEFT = "DPadLeft_";
  public static final String KEY_GCBIND_DPAD_RIGHT = "DPadRight_";

  public static final String KEY_GCADAPTER_RUMBLE = "AdapterRumble";
  public static final String KEY_GCADAPTER_BONGOS = "SimulateKonga";

  public static final String KEY_WIIMOTE_TYPE = "Source";
  public static final String KEY_WIIMOTE_EXTENSION = "Extension";

  public static final String KEY_WIIBIND_A = "WiimoteA_";
  public static final String KEY_WIIBIND_B = "WiimoteB_";
  public static final String KEY_WIIBIND_1 = "Wiimote1_";
  public static final String KEY_WIIBIND_2 = "Wiimote2_";
  public static final String KEY_WIIBIND_MINUS = "WiimoteMinus_";
  public static final String KEY_WIIBIND_PLUS = "WiimotePlus_";
  public static final String KEY_WIIBIND_HOME = "WiimoteHome_";
  public static final String KEY_WIIBIND_IR_UP = "IRUp_";
  public static final String KEY_WIIBIND_IR_DOWN = "IRDown_";
  public static final String KEY_WIIBIND_IR_LEFT = "IRLeft_";
  public static final String KEY_WIIBIND_IR_RIGHT = "IRRight_";
  public static final String KEY_WIIBIND_IR_FORWARD = "IRForward_";
  public static final String KEY_WIIBIND_IR_BACKWARD = "IRBackward_";
  public static final String KEY_WIIBIND_IR_HIDE = "IRHide_";
  public static final String KEY_WIIBIND_SWING_UP = "SwingUp_";
  public static final String KEY_WIIBIND_SWING_DOWN = "SwingDown_";
  public static final String KEY_WIIBIND_SWING_LEFT = "SwingLeft_";
  public static final String KEY_WIIBIND_SWING_RIGHT = "SwingRight_";
  public static final String KEY_WIIBIND_SWING_FORWARD = "SwingForward_";
  public static final String KEY_WIIBIND_SWING_BACKWARD = "SwingBackward_";
  public static final String KEY_WIIBIND_TILT_FORWARD = "TiltForward_";
  public static final String KEY_WIIBIND_TILT_BACKWARD = "TiltBackward_";
  public static final String KEY_WIIBIND_TILT_LEFT = "TiltLeft_";
  public static final String KEY_WIIBIND_TILT_RIGHT = "TiltRight_";
  public static final String KEY_WIIBIND_TILT_MODIFIER = "TiltModifier_";
  public static final String KEY_WIIBIND_SHAKE_X = "ShakeX_";
  public static final String KEY_WIIBIND_SHAKE_Y = "ShakeY_";
  public static final String KEY_WIIBIND_SHAKE_Z = "ShakeZ_";
  public static final String KEY_WIIBIND_DPAD_UP = "WiimoteUp_";
  public static final String KEY_WIIBIND_DPAD_DOWN = "WiimoteDown_";
  public static final String KEY_WIIBIND_DPAD_LEFT = "WiimoteLeft_";
  public static final String KEY_WIIBIND_DPAD_RIGHT = "WiimoteRight_";
  public static final String KEY_WIIBIND_NUNCHUK_C = "NunchukC_";
  public static final String KEY_WIIBIND_NUNCHUK_Z = "NunchukZ_";
  public static final String KEY_WIIBIND_NUNCHUK_UP = "NunchukUp_";
  public static final String KEY_WIIBIND_NUNCHUK_DOWN = "NunchukDown_";
  public static final String KEY_WIIBIND_NUNCHUK_LEFT = "NunchukLeft_";
  public static final String KEY_WIIBIND_NUNCHUK_RIGHT = "NunchukRight_";
  public static final String KEY_WIIBIND_NUNCHUK_SWING_UP = "NunchukSwingUp_";
  public static final String KEY_WIIBIND_NUNCHUK_SWING_DOWN = "NunchukSwingDown_";
  public static final String KEY_WIIBIND_NUNCHUK_SWING_LEFT = "NunchukSwingLeft_";
  public static final String KEY_WIIBIND_NUNCHUK_SWING_RIGHT = "NunchukSwingRight_";
  public static final String KEY_WIIBIND_NUNCHUK_SWING_FORWARD = "NunchukSwingForward_";
  public static final String KEY_WIIBIND_NUNCHUK_SWING_BACKWARD = "NunchukSwingBackward_";
  public static final String KEY_WIIBIND_NUNCHUK_TILT_FORWARD = "NunchukTiltForward_";
  public static final String KEY_WIIBIND_NUNCHUK_TILT_BACKWARD = "NunchukTiltBackward_";
  public static final String KEY_WIIBIND_NUNCHUK_TILT_LEFT = "NunchukTiltLeft_";
  public static final String KEY_WIIBIND_NUNCHUK_TILT_RIGHT = "NunchukTiltRight_";
  public static final String KEY_WIIBIND_NUNCHUK_TILT_MODIFIER = "NunchukTiltModifier_";
  public static final String KEY_WIIBIND_NUNCHUK_SHAKE_X = "NunchukShakeX_";
  public static final String KEY_WIIBIND_NUNCHUK_SHAKE_Y = "NunchukShakeY_";
  public static final String KEY_WIIBIND_NUNCHUK_SHAKE_Z = "NunchukShakeZ_";
  public static final String KEY_WIIBIND_CLASSIC_A = "ClassicA_";
  public static final String KEY_WIIBIND_CLASSIC_B = "ClassicB_";
  public static final String KEY_WIIBIND_CLASSIC_X = "ClassicX_";
  public static final String KEY_WIIBIND_CLASSIC_Y = "ClassicY_";
  public static final String KEY_WIIBIND_CLASSIC_ZL = "ClassicZL_";
  public static final String KEY_WIIBIND_CLASSIC_ZR = "ClassicZR_";
  public static final String KEY_WIIBIND_CLASSIC_MINUS = "ClassicMinus_";
  public static final String KEY_WIIBIND_CLASSIC_PLUS = "ClassicPlus_";
  public static final String KEY_WIIBIND_CLASSIC_HOME = "ClassicHome_";
  public static final String KEY_WIIBIND_CLASSIC_LEFT_UP = "ClassicLeftStickUp_";
  public static final String KEY_WIIBIND_CLASSIC_LEFT_DOWN = "ClassicLeftStickDown_";
  public static final String KEY_WIIBIND_CLASSIC_LEFT_LEFT = "ClassicLeftStickLeft_";
  public static final String KEY_WIIBIND_CLASSIC_LEFT_RIGHT = "ClassicLeftStickRight_";
  public static final String KEY_WIIBIND_CLASSIC_RIGHT_UP = "ClassicRightStickUp_";
  public static final String KEY_WIIBIND_CLASSIC_RIGHT_DOWN = "ClassicRightStickDown_";
  public static final String KEY_WIIBIND_CLASSIC_RIGHT_LEFT = "ClassicRightStickLeft_";
  public static final String KEY_WIIBIND_CLASSIC_RIGHT_RIGHT = "ClassicRightStickRight_";
  public static final String KEY_WIIBIND_CLASSIC_TRIGGER_L = "ClassicTriggerL_";
  public static final String KEY_WIIBIND_CLASSIC_TRIGGER_R = "ClassicTriggerR_";
  public static final String KEY_WIIBIND_CLASSIC_DPAD_UP = "ClassicUp_";
  public static final String KEY_WIIBIND_CLASSIC_DPAD_DOWN = "ClassicDown_";
  public static final String KEY_WIIBIND_CLASSIC_DPAD_LEFT = "ClassicLeft_";
  public static final String KEY_WIIBIND_CLASSIC_DPAD_RIGHT = "ClassicRight_";
  public static final String KEY_WIIBIND_GUITAR_FRET_GREEN = "GuitarGreen_";
  public static final String KEY_WIIBIND_GUITAR_FRET_RED = "GuitarRed_";
  public static final String KEY_WIIBIND_GUITAR_FRET_YELLOW = "GuitarYellow_";
  public static final String KEY_WIIBIND_GUITAR_FRET_BLUE = "GuitarBlue_";
  public static final String KEY_WIIBIND_GUITAR_FRET_ORANGE = "GuitarOrange_";
  public static final String KEY_WIIBIND_GUITAR_STRUM_UP = "GuitarStrumUp_";
  public static final String KEY_WIIBIND_GUITAR_STRUM_DOWN = "GuitarStrumDown_";
  public static final String KEY_WIIBIND_GUITAR_MINUS = "GuitarMinus_";
  public static final String KEY_WIIBIND_GUITAR_PLUS = "GuitarPlus_";
  public static final String KEY_WIIBIND_GUITAR_STICK_UP = "GuitarUp_";
  public static final String KEY_WIIBIND_GUITAR_STICK_DOWN = "GuitarDown_";
  public static final String KEY_WIIBIND_GUITAR_STICK_LEFT = "GuitarLeft_";
  public static final String KEY_WIIBIND_GUITAR_STICK_RIGHT = "GuitarRight_";
  public static final String KEY_WIIBIND_GUITAR_WHAMMY_BAR = "GuitarWhammy_";
  public static final String KEY_WIIBIND_DRUMS_PAD_RED = "DrumsRed_";
  public static final String KEY_WIIBIND_DRUMS_PAD_YELLOW = "DrumsYellow_";
  public static final String KEY_WIIBIND_DRUMS_PAD_BLUE = "DrumsBlue_";
  public static final String KEY_WIIBIND_DRUMS_PAD_GREEN = "DrumsGreen_";
  public static final String KEY_WIIBIND_DRUMS_PAD_ORANGE = "DrumsOrange_";
  public static final String KEY_WIIBIND_DRUMS_PAD_BASS = "DrumsBass_";
  public static final String KEY_WIIBIND_DRUMS_MINUS = "DrumsMinus_";
  public static final String KEY_WIIBIND_DRUMS_PLUS = "DrumsPlus_";
  public static final String KEY_WIIBIND_DRUMS_STICK_UP = "DrumsUp_";
  public static final String KEY_WIIBIND_DRUMS_STICK_DOWN = "DrumsDown_";
  public static final String KEY_WIIBIND_DRUMS_STICK_LEFT = "DrumsLeft_";
  public static final String KEY_WIIBIND_DRUMS_STICK_RIGHT = "DrumsRight_";
  public static final String KEY_WIIBIND_TURNTABLE_GREEN_LEFT = "TurntableGreenLeft_";
  public static final String KEY_WIIBIND_TURNTABLE_RED_LEFT = "TurntableRedLeft_";
  public static final String KEY_WIIBIND_TURNTABLE_BLUE_LEFT = "TurntableBlueLeft_";
  public static final String KEY_WIIBIND_TURNTABLE_GREEN_RIGHT = "TurntableGreenRight_";
  public static final String KEY_WIIBIND_TURNTABLE_RED_RIGHT = "TurntableRedRight_";
  public static final String KEY_WIIBIND_TURNTABLE_BLUE_RIGHT = "TurntableBlueRight_";
  public static final String KEY_WIIBIND_TURNTABLE_MINUS = "TurntableMinus_";
  public static final String KEY_WIIBIND_TURNTABLE_PLUS = "TurntablePlus_";
  public static final String KEY_WIIBIND_TURNTABLE_EUPHORIA = "TurntableEuphoria_";
  public static final String KEY_WIIBIND_TURNTABLE_LEFT_LEFT = "TurntableLeftLeft_";
  public static final String KEY_WIIBIND_TURNTABLE_LEFT_RIGHT = "TurntableLeftRight_";
  public static final String KEY_WIIBIND_TURNTABLE_RIGHT_LEFT = "TurntableRightLeft_";
  public static final String KEY_WIIBIND_TURNTABLE_RIGHT_RIGHT = "TurntableRightRight_";
  public static final String KEY_WIIBIND_TURNTABLE_STICK_UP = "TurntableUp_";
  public static final String KEY_WIIBIND_TURNTABLE_STICK_DOWN = "TurntableDown_";
  public static final String KEY_WIIBIND_TURNTABLE_STICK_LEFT = "TurntableLeft_";
  public static final String KEY_WIIBIND_TURNTABLE_STICK_RIGHT = "TurntableRight_";
  public static final String KEY_WIIBIND_TURNTABLE_EFFECT_DIAL = "TurntableEffDial_";
  public static final String KEY_WIIBIND_TURNTABLE_CROSSFADE_LEFT = "TurntableCrossLeft_";
  public static final String KEY_WIIBIND_TURNTABLE_CROSSFADE_RIGHT = "TurntableCrossRight_";

  public static final String KEY_WIIMOTE_SCAN = "WiimoteContinuousScanning";
  public static final String KEY_WIIMOTE_SPEAKER = "WiimoteEnableSpeaker";

  // Internal only, not actually found in settings file.
  public static final String KEY_VIDEO_BACKEND_INDEX = "VideoBackendIndex";

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
   * Reads a given .ini file from disk and returns it as a HashMap of Settings, themselves
   * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
   * failed.
   *
   * @param ini  The ini file to load the settings from
   * @param view The current view.
   */
  static HashMap<String, SettingSection> readFile(final File ini, boolean isCustomGame,
          SettingsActivityView view)
  {
    HashMap<String, SettingSection> sections = new Settings.SettingsSectionMap();

    BufferedReader reader = null;

    try
    {
      reader = new BufferedReader(new FileReader(ini));

      SettingSection current = null;
      for (String line; (line = reader.readLine()) != null; )
      {
        if (line.startsWith("[") && line.endsWith("]"))
        {
          current = sectionFromLine(line, isCustomGame);
          sections.put(current.getName(), current);
        }
        else if ((current != null))
        {
          Setting setting = settingFromLine(current, line);
          if (setting != null)
          {
            current.putSetting(setting);
          }
        }
      }
    }
    catch (FileNotFoundException e)
    {
      Log.error("[SettingsFile] File not found: " + ini.getAbsolutePath() + e.getMessage());
      if (view != null)
        view.onSettingsFileNotFound();
    }
    catch (IOException e)
    {
      Log.error("[SettingsFile] Error reading from: " + ini.getAbsolutePath() + e.getMessage());
      if (view != null)
        view.onSettingsFileNotFound();
    }
    finally
    {
      if (reader != null)
      {
        try
        {
          reader.close();
        }
        catch (IOException e)
        {
          Log.error("[SettingsFile] Error closing: " + ini.getAbsolutePath() + e.getMessage());
        }
      }
    }

    return sections;
  }

  public static HashMap<String, SettingSection> readFile(final String fileName,
          SettingsActivityView view)
  {
    HashMap<String, SettingSection> sections = readFile(getSettingsFile(fileName), false, view);

    if (fileName.equals(SettingsFile.FILE_NAME_DOLPHIN))
    {
      addGcPadSettingsIfTheyDontExist(sections);
    }

    return sections;
  }

  /**
   * Reads a given .ini file from disk and returns it as a HashMap of SettingSections, themselves
   * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
   * failed.
   *
   * @param gameId the id of the game to load it's settings.
   * @param view   The current view.
   */
  public static HashMap<String, SettingSection> readCustomGameSettings(final String gameId,
          SettingsActivityView view)
  {
    return readFile(getCustomGameSettingsFile(gameId), true, view);
  }

  public static HashMap<String, SettingSection> readGenericGameSettings(final String gameId,
          SettingsActivityView view)
  {
    return readFile(getGenericGameSettingsFile(gameId), true, view);
  }

  public static HashMap<String, SettingSection> readGenericGameSettingsForAllRegions(
          final String gameId, SettingsActivityView view)
  {
    return readFile(getGenericGameSettingsForAllRegions(gameId), true, view);
  }


  /**
   * Saves a Settings HashMap to a given .ini file on disk. If unsuccessful, outputs an error
   * telling why it failed.
   *
   * @param fileName The target filename without a path or extension.
   * @param sections The HashMap containing the Settings we want to serialize.
   * @param view     The current view.
   */
  public static void saveFile(final String fileName, TreeMap<String, SettingSection> sections,
          SettingsActivityView view)
  {
    File ini = getSettingsFile(fileName);

    PrintWriter writer = null;
    try
    {
      writer = new PrintWriter(ini, "UTF-8");

      Set<String> keySet = sections.keySet();
      Set<String> sortedKeySet = new TreeSet<>(keySet);

      for (String key : sortedKeySet)
      {
        SettingSection section = sections.get(key);
        writeSection(writer, section);
      }
    }
    catch (FileNotFoundException e)
    {
      Log.error("[SettingsFile] File not found: " + fileName + ".ini: " + e.getMessage());
      if (view != null)
        view.showToastMessage("Error saving " + fileName + ".ini: " + e.getMessage());
    }
    catch (UnsupportedEncodingException e)
    {
      Log.error("[SettingsFile] Bad encoding; please file a bug report: " + fileName + ".ini: " +
              e.getMessage());
      if (view != null)
        view.showToastMessage("Error saving " + fileName + ".ini: " + e.getMessage());
    }
    finally
    {
      if (writer != null)
      {
        writer.close();
      }
    }
  }

  public static void saveCustomGameSettings(final String gameId,
          final HashMap<String, SettingSection> sections)
  {
    Set<String> sortedSections = new TreeSet<>(sections.keySet());

    for (String sectionKey : sortedSections)
    {
      SettingSection section = sections.get(sectionKey);

      HashMap<String, Setting> settings = section.getSettings();
      Set<String> sortedKeySet = new TreeSet<>(settings.keySet());

      for (String settingKey : sortedKeySet)
      {
        Setting setting = settings.get(settingKey);
        NativeLibrary
                .SetUserSetting(gameId, mapSectionNameFromIni(section.getName()), setting.getKey(),
                        setting.getValueAsString());
      }
    }
  }

  private static String mapSectionNameFromIni(String generalSectionName)
  {
    if (sectionsMap.getForward(generalSectionName) != null)
    {
      return sectionsMap.getForward(generalSectionName);
    }

    return generalSectionName;
  }

  private static String mapSectionNameToIni(String generalSectionName)
  {
    if (sectionsMap.getBackward(generalSectionName) != null)
    {
      return sectionsMap.getBackward(generalSectionName);
    }

    return generalSectionName;
  }

  @NonNull
  private static File getSettingsFile(String fileName)
  {
    return new File(
            DirectoryInitialization.getUserDirectory() + "/Config/" + fileName + ".ini");
  }

  private static File getGenericGameSettingsForAllRegions(String gameId)
  {
    // Use the first 3 chars from the gameId to load the generic game settings for all regions
    gameId = gameId.substring(0, 3);
    return new File(
            DirectoryInitialization.getDolphinInternalDirectory() + "/GameSettings/" +
                    gameId + ".ini");
  }

  private static File getGenericGameSettingsFile(String gameId)
  {
    return new File(
            DirectoryInitialization.getDolphinInternalDirectory() + "/GameSettings/" +
                    gameId + ".ini");
  }

  private static File getCustomGameSettingsFile(String gameId)
  {
    return new File(
            DirectoryInitialization.getUserDirectory() + "/GameSettings/" + gameId + ".ini");
  }

  private static SettingSection sectionFromLine(String line, boolean isCustomGame)
  {
    String sectionName = line.substring(1, line.length() - 1);
    if (isCustomGame)
    {
      sectionName = mapSectionNameToIni(sectionName);
    }
    return new SettingSection(sectionName);
  }

  private static void addGcPadSettingsIfTheyDontExist(HashMap<String, SettingSection> sections)
  {
    SettingSection coreSection = sections.get(Settings.SECTION_INI_CORE);

    for (int i = 0; i < 4; i++)
    {
      String key = SettingsFile.KEY_GCPAD_TYPE + i;
      if (coreSection.getSetting(key) == null)
      {
        // Set GameCube controller 1 to enabled, all others disabled
        Setting gcPadSetting = new IntSetting(key, Settings.SECTION_INI_CORE, i == 0 ? 6 : 0);
        coreSection.putSetting(gcPadSetting);
      }
    }

    sections.put(Settings.SECTION_INI_CORE, coreSection);
  }

  public static void firstAnalyticsAdd(boolean enabled)
  {
    HashMap<String, SettingSection> dolphinSections =
            readFile(SettingsFile.FILE_NAME_DOLPHIN, null);
    SettingSection analyticsSection = dolphinSections.get(Settings.SECTION_ANALYTICS);

    Setting analyticsEnabled = new StringSetting(KEY_ANALYTICS_ENABLED, Settings.SECTION_ANALYTICS,
            enabled ? "True" : "False");
    Setting analyticsFirstAsk =
            new StringSetting(KEY_ANALYTICS_PERMISSION_ASKED, Settings.SECTION_ANALYTICS, "True");

    analyticsSection.putSetting(analyticsFirstAsk);
    analyticsSection.putSetting(analyticsEnabled);

    dolphinSections.put(Settings.SECTION_ANALYTICS, analyticsSection);

    TreeMap<String, SettingSection> saveSection = new TreeMap<>(dolphinSections);
    saveFile(SettingsFile.FILE_NAME_DOLPHIN, saveSection, null);
  }

  /**
   * For a line of text, determines what type of data is being represented, and returns
   * a Setting object containing this data.
   *
   * @param current The section currently being parsed by the consuming method.
   * @param line    The line of text being parsed.
   * @return A typed Setting containing the key/value contained in the line.
   */
  private static Setting settingFromLine(SettingSection current, String line)
  {
    String[] splitLine = line.split("=");

    if (splitLine.length != 2)
    {
      Log.warning("Skipping invalid config line \"" + line + "\"");
      return null;
    }

    String key = splitLine[0].trim();
    String value = splitLine[1].trim();

    try
    {
      int valueAsInt = Integer.valueOf(value);

      return new IntSetting(key, current.getName(), valueAsInt);
    }
    catch (NumberFormatException ex)
    {
    }

    try
    {
      float valueAsFloat = Float.valueOf(value);

      return new FloatSetting(key, current.getName(), valueAsFloat);
    }
    catch (NumberFormatException ex)
    {
    }

    switch (value)
    {
      case "True":
        return new BooleanSetting(key, current.getName(), true);
      case "False":
        return new BooleanSetting(key, current.getName(), false);
      default:
        return new StringSetting(key, current.getName(), value);
    }
  }

  /**
   * Writes the contents of a Section HashMap to disk.
   *
   * @param writer  A PrintWriter pointed at a file on disk.
   * @param section A section containing settings to be written to the file.
   */
  private static void writeSection(PrintWriter writer, SettingSection section)
  {
    // Write the section header.
    String header = sectionAsString(section);
    writer.println(header);

    // Write this section's values.
    HashMap<String, Setting> settings = section.getSettings();
    Set<String> keySet = settings.keySet();
    Set<String> sortedKeySet = new TreeSet<>(keySet);

    for (String key : sortedKeySet)
    {
      Setting setting = settings.get(key);
      String settingString = settingAsString(setting);

      writer.println(settingString);
    }
  }

  private static String sectionAsString(SettingSection section)
  {
    return "[" + section.getName() + "]";
  }

  private static String settingAsString(Setting setting)
  {
    return setting.getKey() + " = " + setting.getValueAsString();
  }
}
