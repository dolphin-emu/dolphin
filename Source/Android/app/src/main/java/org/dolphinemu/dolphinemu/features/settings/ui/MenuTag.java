// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import androidx.annotation.NonNull;

public enum MenuTag
{
  SETTINGS("settings"),
  CONFIG("config"),
  CONFIG_GENERAL("config_general"),
  CONFIG_INTERFACE("config_interface"),
  CONFIG_AUDIO("config_audio"),
  CONFIG_PATHS("config_paths"),
  CONFIG_GAME_CUBE("config_gamecube"),
  CONFIG_SERIALPORT1("config_serialport1"),
  CONFIG_WII("config_wii"),
  CONFIG_ADVANCED("config_advanced"),
  CONFIG_LOG("config_log"),
  DEBUG("debug"),
  GRAPHICS("graphics"),
  ENHANCEMENTS("enhancements"),
  STEREOSCOPY("stereoscopy"),
  HACKS("hacks"),
  ADVANCED_GRAPHICS("advanced_graphics"),
  GCPAD_TYPE("gc_pad_type"),
  WIIMOTE("wiimote"),
  WIIMOTE_EXTENSION("wiimote_extension"),
  GCPAD_1("gcpad", 0),
  GCPAD_2("gcpad", 1),
  GCPAD_3("gcpad", 2),
  GCPAD_4("gcpad", 3),
  WIIMOTE_1("wiimote", 4),
  WIIMOTE_2("wiimote", 5),
  WIIMOTE_3("wiimote", 6),
  WIIMOTE_4("wiimote", 7),
  WIIMOTE_EXTENSION_1("wiimote_extension", 4),
  WIIMOTE_EXTENSION_2("wiimote_extension", 5),
  WIIMOTE_EXTENSION_3("wiimote_extension", 6),
  WIIMOTE_EXTENSION_4("wiimote_extension", 7);

  private String tag;
  private int subType = -1;

  MenuTag(String tag)
  {
    this.tag = tag;
  }

  MenuTag(String tag, int subtype)
  {
    this.tag = tag;
    this.subType = subtype;
  }

  @NonNull
  @Override
  public String toString()
  {
    if (subType != -1)
    {
      return tag + subType;
    }

    return tag;
  }

  public String getTag()
  {
    return tag;
  }

  public int getSubType()
  {
    return subType;
  }

  public boolean isSerialPort1Menu()
  {
    return this == CONFIG_SERIALPORT1;
  }

  public boolean isGCPadMenu()
  {
    return this == GCPAD_1 || this == GCPAD_2 || this == GCPAD_3 || this == GCPAD_4;
  }

  public boolean isWiimoteMenu()
  {
    return this == WIIMOTE_1 || this == WIIMOTE_2 || this == WIIMOTE_3 || this == WIIMOTE_4;
  }

  public boolean isWiimoteExtensionMenu()
  {
    return this == WIIMOTE_EXTENSION_1 || this == WIIMOTE_EXTENSION_2
            || this == WIIMOTE_EXTENSION_3 || this == WIIMOTE_EXTENSION_4;
  }

  public static MenuTag getGCPadMenuTag(int subtype)
  {
    return getMenuTag("gcpad", subtype);
  }

  public static MenuTag getWiimoteMenuTag(int subtype)
  {
    return getMenuTag("wiimote", subtype);
  }

  public static MenuTag getWiimoteExtensionMenuTag(int subtype)
  {
    return getMenuTag("wiimote_extension", subtype);
  }

  private static MenuTag getMenuTag(String tag, int subtype)
  {
    for (MenuTag menuTag : MenuTag.values())
    {
      if (menuTag.tag.equals(tag) && menuTag.subType == subtype) return menuTag;
    }

    throw new IllegalArgumentException("You are asking for a menu that is not available or " +
            "passing a wrong subtype");
  }
}
