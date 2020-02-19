package org.dolphinemu.dolphinemu.utils;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.SettingSection;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.InputBindingSetting;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class GamePadHandler
{
  private static Map<String, Integer> inputs = new HashMap<>();

  public static void load(String filePath)
  {
    Settings settings = new Settings();
    settings.loadSettings(null);
    File file = new File(filePath);
    String fileName = file.getName();
    int iniIndex = fileName.indexOf(".ini");
    if  (iniIndex != -1)
    {
      fileName = fileName.substring(0, iniIndex);
    }
    List<String> contents = readIniFile(file);
    if (contents.isEmpty())
    {
      return;
    }
    for (String input : inputs.keySet())
    {
      for (int i = 0; i < 8; i++)
      {
        clearValue(input + i, settings);
      }
      for (String line : contents)
      {
        if (line.indexOf('=') == -1)
        {
          continue;
        }
        if (!line.startsWith(input))
        {
          continue;
        }
        String[] entry = line.split("=", 2);
        String key = entry[0].trim();
        String value = entry[1].trim();
        setValue(fileName, key, value, settings);
        break;
      }
    }
    settings.saveSettings(null);
  }

  public static void save(String filePath)
  {
    Settings settings = new Settings();
    settings.loadSettings(null);
    SettingSection bindingsSection = settings.getSection(Settings.SECTION_BINDINGS);
    List<String> contents = new ArrayList<>();
    contents.add("[Profile]");
    for (String input : inputs.keySet())
    {
      for (int i = 0; i < 8; i++)
      {
        Setting setting = bindingsSection.getSetting(input + i);
        if (setting == null)
        {
          continue;
        }
        contents.add(setting.getKey() + " = " + setting.getValueAsString());
      }
    }
    if (contents.size() == 1)
    {
      return;
    }
    saveIniFile(new File(filePath), contents);
  }

  private static void setValue(String deviceName, String key, String value, Settings settings)
  {
    InputBindingSetting binding = getBinding(key, settings);
    if (binding == null)
    {
      return;
    }
    String ui = deviceName + ": Button " +  value.substring(value.indexOf("'-") + 1);
    binding.setValue(value, ui);
    putSetting(binding, settings);
  }

  private static void clearValue(String key, Settings settings)
  {
    InputBindingSetting binding = getBinding(key, settings);
    if (binding == null)
    {
      return;
    }
    binding.setValue("", "");
    putSetting(binding, settings);
  }

  private static InputBindingSetting getBinding(String key, Settings settings)
  {
    String basicKey = getBasicKeyName(key);
    Integer inputPos = inputs.get(basicKey);
    if (inputPos == null)
    {
      return null;
    }
    Setting sectionSetting = settings.getSection(Settings.SECTION_BINDINGS).getSetting(key);
    return new InputBindingSetting(key, Settings.SECTION_BINDINGS, inputPos, sectionSetting, "");
  }

  private static void putSetting(InputBindingSetting binding, Settings settings)
  {
    StringSetting setting = new StringSetting(binding.getKey(), binding.getSection(),
            binding.getValue());
    settings.getSection(setting.getSection()).putSetting(setting);
  }

  private static String getBasicKeyName(String key)
  {
    int underscoreIndex = key.lastIndexOf('_');
    if (underscoreIndex != -1)
    {
      key = key.substring(0, key.lastIndexOf('_') + 1);
    }
    if (key.matches(".*[0-9]"))
    {
      key = key.substring(0, key.length() - 1);
    }
    return key;
  }

  private static List<String> readIniFile(File file)
  {
    if (!file.exists())
    {
      return null;
    }
    List<String> contents = new ArrayList<>();
    try (BufferedReader br = new BufferedReader(new FileReader(file)))
    {
      String line;
      while ((line = br.readLine()) != null)
      {
        contents.add(line);
      }
    }
    catch (FileNotFoundException e)
    {
      e.printStackTrace();
    }
    catch (IOException e)
    {
      e.printStackTrace();
    }
    return contents;
  }

  public static void saveIniFile(File file, List<String> contents)
  {
    if (!file.getName().endsWith(".ini"))
    {
      file = new File(file.getAbsolutePath() + ".ini");
    }
    try (BufferedWriter bw = new BufferedWriter(new FileWriter(file)))
    {
      for (String line : contents)
      {
        bw.append(line);
        bw.newLine();
      }
      bw.flush();
    }
    catch (IOException e)
    {
      e.printStackTrace();
    }
  }

  static
  {
    inputs.put(SettingsFile.KEY_GCBIND_A, R.string.button_a);
    inputs.put(SettingsFile.KEY_GCBIND_B, R.string.button_b);
    inputs.put(SettingsFile.KEY_GCBIND_X, R.string.button_x);
    inputs.put(SettingsFile.KEY_GCBIND_Y, R.string.button_y);
    inputs.put(SettingsFile.KEY_GCBIND_Z, R.string.button_z);
    inputs.put(SettingsFile.KEY_GCBIND_START, R.string.button_start);

    inputs.put(SettingsFile.KEY_GCBIND_CONTROL_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_GCBIND_CONTROL_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_GCBIND_CONTROL_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_GCBIND_CONTROL_RIGHT, R.string.generic_right);

    inputs.put(SettingsFile.KEY_GCBIND_C_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_GCBIND_C_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_GCBIND_C_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_GCBIND_C_RIGHT, R.string.generic_right);

    inputs.put(SettingsFile.KEY_GCBIND_TRIGGER_L, R.string.trigger_left);
    inputs.put(SettingsFile.KEY_GCBIND_TRIGGER_R, R.string.trigger_right);

    inputs.put(SettingsFile.KEY_GCBIND_DPAD_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_GCBIND_DPAD_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_GCBIND_DPAD_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_GCBIND_DPAD_RIGHT, R.string.generic_right);
    inputs.put(SettingsFile.KEY_EMU_RUMBLE, R.string.emulation_control_rumble);


    inputs.put(SettingsFile.KEY_WIIMOTE_EXTENSION, R.string.wiimote_extensions);

    inputs.put(SettingsFile.KEY_WIIBIND_A, R.string.button_a);
    inputs.put(SettingsFile.KEY_WIIBIND_B, R.string.button_b);
    inputs.put(SettingsFile.KEY_WIIBIND_1, R.string.button_one);
    inputs.put(SettingsFile.KEY_WIIBIND_2, R.string.button_two);
    inputs.put(SettingsFile.KEY_WIIBIND_MINUS, R.string.button_minus);
    inputs.put(SettingsFile.KEY_WIIBIND_PLUS, R.string.button_plus);
    inputs.put(SettingsFile.KEY_WIIBIND_HOME, R.string.button_home);

    inputs.put(SettingsFile.KEY_WIIBIND_IR_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_WIIBIND_IR_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_WIIBIND_IR_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_IR_RIGHT, R.string.generic_right);
    inputs.put(SettingsFile.KEY_WIIBIND_IR_FORWARD, R.string.generic_forward);
    inputs.put(SettingsFile.KEY_WIIBIND_IR_BACKWARD, R.string.generic_backward);
    inputs.put(SettingsFile.KEY_WIIBIND_IR_HIDE, R.string.ir_hide);

    inputs.put(SettingsFile.KEY_WIIBIND_SWING_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_WIIBIND_SWING_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_WIIBIND_SWING_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_SWING_RIGHT, R.string.generic_right);
    inputs.put(SettingsFile.KEY_WIIBIND_SWING_FORWARD, R.string.generic_forward);
    inputs.put(SettingsFile.KEY_WIIBIND_SWING_BACKWARD, R.string.generic_backward);

    inputs.put(SettingsFile.KEY_WIIBIND_TILT_FORWARD, R.string.generic_forward);
    inputs.put(SettingsFile.KEY_WIIBIND_TILT_BACKWARD, R.string.generic_backward);
    inputs.put(SettingsFile.KEY_WIIBIND_TILT_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_TILT_RIGHT, R.string.generic_right);
    inputs.put(SettingsFile.KEY_WIIBIND_TILT_MODIFIER, R.string.tilt_modifier);

    inputs.put(SettingsFile.KEY_WIIBIND_SHAKE_X, R.string.shake_x);
    inputs.put(SettingsFile.KEY_WIIBIND_SHAKE_Y, R.string.shake_y);
    inputs.put(SettingsFile.KEY_WIIBIND_SHAKE_Z, R.string.shake_z);

    inputs.put(SettingsFile.KEY_WIIBIND_DPAD_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_WIIBIND_DPAD_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_WIIBIND_DPAD_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_DPAD_RIGHT, R.string.generic_right);
    inputs.put(SettingsFile.KEY_EMU_RUMBLE, R.string.emulation_control_rumble);


    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_C, R.string.nunchuk_button_c);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_Z, R.string.button_z);

    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_RIGHT, R.string.generic_right);

    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_RIGHT, R.string.generic_right);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_FORWARD, R.string.generic_forward);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_BACKWARD, R.string.generic_backward);

    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_FORWARD, R.string.generic_forward);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_BACKWARD, R.string.generic_backward);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_RIGHT, R.string.generic_right);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_MODIFIER, R.string.tilt_modifier);

    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_X, R.string.shake_x);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Y, R.string.shake_y);
    inputs.put(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Z, R.string.shake_z);


    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_A, R.string.button_a);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_B, R.string.button_b);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_X, R.string.button_x);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_Y, R.string.button_y);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_ZL, R.string.classic_button_zl);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_ZR, R.string.classic_button_zr);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_MINUS, R.string.button_minus);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_PLUS, R.string.button_plus);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_HOME, R.string.button_home);

    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_RIGHT, R.string.generic_right);

    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_RIGHT, R.string.generic_right);

    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_L, R.string.trigger_left);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_R, R.string.trigger_right);

    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_UP, R.string.generic_up);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_DOWN, R.string.generic_down);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_LEFT, R.string.generic_left);
    inputs.put(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_RIGHT, R.string.generic_right);
  }
}
