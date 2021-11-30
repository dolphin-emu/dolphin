// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui;

public class RiivolutionItem
{
  public final int mDiscIndex;
  public final int mSectionIndex;
  public final int mOptionIndex;

  /**
   * Constructor for a disc.
   */
  public RiivolutionItem(int discIndex)
  {
    mDiscIndex = discIndex;
    mSectionIndex = -1;
    mOptionIndex = -1;
  }

  /**
   * Constructor for a section.
   */
  public RiivolutionItem(int discIndex, int sectionIndex)
  {
    mDiscIndex = discIndex;
    mSectionIndex = sectionIndex;
    mOptionIndex = -1;
  }

  /**
   * Constructor for an option.
   */
  public RiivolutionItem(int discIndex, int sectionIndex, int optionIndex)
  {
    mDiscIndex = discIndex;
    mSectionIndex = sectionIndex;
    mOptionIndex = optionIndex;
  }
}
