// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.model;

public class Skylander
{
  private SkylanderPair mPair;
  private String mName;

  public final static Skylander BLANK_SKYLANDER = new Skylander(-1, -1, "Blank");

  public Skylander(int id, int var, String name)
  {
    mPair = new SkylanderPair(id, var);
    mName = name;
  }

  public String getName()
  {
    return mName;
  }

  public void setName(String name)
  {
    this.mName = name;
  }

  public int getId()
  {
    return mPair.getId();
  }

  public int getVar()
  {
    return mPair.getVar();
  }
}
