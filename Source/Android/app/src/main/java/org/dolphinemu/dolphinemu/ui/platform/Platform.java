package org.dolphinemu.dolphinemu.ui.platform;

/**
 * Enum to represent platform (eg GameCube, Wii).
 */
public enum Platform
{
  GAMECUBE(0, "GameCube Games"),
  WII(1, "Wii Games"),
  WIIWARE(2, "WiiWare Games");

  private final int value;
  private final String headerName;

  Platform(int value, String headerName)
  {
    this.value = value;
    this.headerName = headerName;
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

  public String getHeaderName()
  {
    return headerName;
  }
}
