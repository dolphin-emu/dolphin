// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

// This stuff is pretty ugly. It's a kind of workaround for certain controller settings
// not actually being available as game-specific settings.
public class WiimoteProfileBooleanSetting implements AbstractBooleanSetting
{
  private final int mPadID;

  private final String mSection;
  private final String mKey;
  private final boolean mDefaultValue;

  private final String mProfileKey;

  private final String mProfile;

  public WiimoteProfileBooleanSetting(String gameID, int padID, String section, String key,
          boolean defaultValue)
  {
    mPadID = padID;
    mSection = section;
    mKey = key;
    mDefaultValue = defaultValue;

    mProfileKey = SettingsFile.KEY_WIIMOTE_PROFILE + (padID + 1);

    mProfile = gameID + "_Wii" + padID;
  }

  @Override
  public boolean isOverridden(Settings settings)
  {
    return settings.isWiimoteProfileEnabled(settings, mProfile, mProfileKey);
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return false;
  }

  @Override
  public boolean delete(Settings settings)
  {
    return settings.disableWiimoteProfile(settings, mProfileKey);
  }

  @Override
  public boolean getBoolean(Settings settings)
  {
    if (settings.isWiimoteProfileEnabled(settings, mProfile, mProfileKey))
      return settings.getWiimoteProfile(mProfile, mPadID).getBoolean(mSection, mKey, mDefaultValue);
    else
      return mDefaultValue;
  }

  @Override
  public void setBoolean(Settings settings, boolean newValue)
  {
    settings.getWiimoteProfile(mProfile, mPadID).setBoolean(mSection, mKey, newValue);

    settings.enableWiimoteProfile(settings, mProfile, mProfileKey);
  }
}
