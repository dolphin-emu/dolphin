package org.dolphinemu.dolphinemu.ui.main;

import org.dolphinemu.dolphinemu.R;

public enum Tab
{
  GAMECUBE(0, "GameCube"),
  WII(1, "Wii"),
  WIIWARE(2, "Wii Ware"),
  MORE(3, "More");

  private final int value;
  private final String idString;

  Tab(int value, String idString)
  {
    this.value = value;
    this.idString = idString;
  }

  public int toInt()
  {
    return value;
  }

  public String getIdString()
  {
    return idString;
  }

  public static int getTabId(int tab)
  {
    if (tab == Tab.GAMECUBE.toInt())
    {
      return R.id.action_gamecube;
    }
    else if (tab == Tab.WII.toInt())
    {
      return R.id.action_wii;
    }
    else if (tab == Tab.WIIWARE.toInt())
    {
      return R.id.action_wiiware;
    }
    else if (tab == Tab.MORE.toInt())
    {
      return R.id.action_more;
    }
    return 0;
  }
}
