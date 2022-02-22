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

  public static final String KEY_GCPAD_PLAYER_1 = "SIDevice0";

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

  public static final String KEY_EMU_RUMBLE = "EmuRumble";

  public static final String KEY_WIIMOTE_EXTENSION = "Extension";

  // Controller keys for game specific settings
  public static final String KEY_WIIMOTE_PROFILE = "WiimoteProfile";

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
  public static final String KEY_WIIBIND_IR_PITCH = "IRTotalPitch";
  public static final String KEY_WIIBIND_IR_YAW = "IRTotalYaw";
  public static final String KEY_WIIBIND_IR_VERTICAL_OFFSET = "IRVerticalOffset";
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

  public static File getWiiProfile(String profile)
  {
    String wiiConfigPath =
            DirectoryInitialization.getUserDirectory() + "/Config/Profiles/Wiimote/" +
                    profile + ".ini";

    return new File(wiiConfigPath);
  }
}
