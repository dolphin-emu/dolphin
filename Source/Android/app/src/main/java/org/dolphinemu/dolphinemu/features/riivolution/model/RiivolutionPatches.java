// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.model;

import androidx.annotation.Keep;

public class RiivolutionPatches
{
  private String mGameId;
  private int mRevision;
  private int mDiscNumber;

  private boolean mUnsavedChanges = false;

  @Keep
  private long mPointer;

  public RiivolutionPatches(String gameId, int revision, int discNumber)
  {
    mGameId = gameId;
    mRevision = revision;
    mDiscNumber = discNumber;

    mPointer = initialize();
  }

  @Override
  public native void finalize();

  private static native long initialize();

  public native int getDiscCount();

  public native String getDiscName(int discIndex);

  public native int getSectionCount(int discIndex);

  public native String getSectionName(int discIndex, int sectionIndex);

  public native int getOptionCount(int discIndex, int sectionIndex);

  public native String getOptionName(int discIndex, int sectionIndex, int optionIndex);

  public native int getChoiceCount(int discIndex, int sectionIndex, int optionIndex);

  public native String getChoiceName(int discIndex, int sectionIndex, int optionIndex,
          int choiceIndex);

  /**
   * @return 0 if no choice is selected, otherwise the index of the selected choice plus one.
   */
  public native int getSelectedChoice(int discIndex, int sectionIndex, int optionIndex);

  /**
   * @param choiceIndex 0 to select no choice, otherwise the choice index plus one.
   */
  public void setSelectedChoice(int discIndex, int sectionIndex, int optionIndex, int choiceIndex)
  {
    mUnsavedChanges = true;
    setSelectedChoiceImpl(discIndex, sectionIndex, optionIndex, choiceIndex);
  }

  /**
   * @param choiceIndex 0 to select no choice, otherwise the choice index plus one.
   */
  private native void setSelectedChoiceImpl(int discIndex, int sectionIndex, int optionIndex,
          int choiceIndex);

  public void loadConfig()
  {
    loadConfigImpl(mGameId, mRevision, mDiscNumber);
  }

  private native void loadConfigImpl(String gameId, int revision, int discNumber);

  public void saveConfig()
  {
    if (mUnsavedChanges)
    {
      mUnsavedChanges = false;
      saveConfigImpl(mGameId);
    }
  }

  private native void saveConfigImpl(String gameId);
}
