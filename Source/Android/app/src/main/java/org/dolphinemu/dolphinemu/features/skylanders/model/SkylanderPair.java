// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.model;

import androidx.annotation.Nullable;

public class SkylanderPair
{
  private int mId;
  private int mVar;

  public SkylanderPair(int id, int var)
  {
    mId = id;
    mVar = var;
  }

  public int getId()
  {
    return mId;
  }

  public void setId(int mId)
  {
    this.mId = mId;
  }

  public int getVar()
  {
    return mVar;
  }

  public void setVar(int mVar)
  {
    this.mVar = mVar;
  }

  @Override public int hashCode()
  {
    return (mId << 16) + mVar;
  }

  @Override public boolean equals(@Nullable Object obj)
  {
    if (!(obj instanceof SkylanderPair))
      return false;
    SkylanderPair pairObj = (SkylanderPair) obj;
    if (pairObj.getId() != mId)
      return false;
    if (pairObj.getVar() != mVar)
      return false;
    return true;
  }
}
