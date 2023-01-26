// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.ui;

public class SkylanderSlot
{
  private String mLabel;
  private final int mSlotNum;
  private int mPortalSlot;

  public SkylanderSlot(String label, int slot)
  {
    mLabel = label;
    mSlotNum = slot;
    mPortalSlot = -1;
  }

  public String getLabel()
  {
    return mLabel;
  }

  public void setLabel(String label)
  {
    mLabel = label;
  }

  public int getSlotNum()
  {
    return mSlotNum;
  }

  public int getPortalSlot()
  {
    return mPortalSlot;
  }

  public void setPortalSlot(int mPortalSlot)
  {
    this.mPortalSlot = mPortalSlot;
  }
}
