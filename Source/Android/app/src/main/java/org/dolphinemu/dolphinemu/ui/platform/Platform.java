// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform;

import org.dolphinemu.dolphinemu.R;

/**
 * Enum to represent platform (eg GameCube, Wii).
 */
public enum Platform
{
  GAMECUBE(0, R.string.platform_gamecube, "GameCube Games"),
  WII(1, R.string.platform_wii, "Wii Games"),
  WIIWARE(2, R.string.platform_wiiware, "WiiWare Games");

  private final int value;
  private final int headerName;
  private final String idString;

  Platform(int value, int headerName, String idString)
  {
    this.value = value;
    this.headerName = headerName;
    this.idString = idString;
  }

  public static Platform fromInt(int i)
  {
    return values()[i];
  }

  public static Platform fromNativeInt(int i)
  {
    // TODO: Proper support for DOL and ELF files
    boolean in_range = i >= 0 && i < values().length;
    return values()[in_range ? i : WIIWARE.value];
  }

  public static Platform fromPosition(int position)
  {
    return values()[position];
  }

  public int toInt()
  {
    return value;
  }

  public int getHeaderName()
  {
    return headerName;
  }

  public String getIdString()
  {
    return idString;
  }
}
