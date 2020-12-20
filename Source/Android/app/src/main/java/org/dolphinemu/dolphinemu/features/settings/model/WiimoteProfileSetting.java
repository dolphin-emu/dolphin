package org.dolphinemu.dolphinemu.features.settings.model;

import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.IniFile;

import java.io.File;

// This stuff is pretty ugly. It's a kind of workaround for certain controller settings
// not actually being available as game-specific settings.
//
// Warning: The current implementation of this should only be used for one setting at a time.
// Because each instance of this class keeps an IniFile around in memory, saving one profile
// setting to disk might overwrite changes that were made to other profile settings earlier.
// This system shouldn't be used for new settings anyway since the approach is fundamentally messy.
public class WiimoteProfileSetting implements AbstractStringSetting
{
  private final int mPadID;

  private final String mSection;
  private final String mKey;
  private final String mDefaultValue;

  private final String mProfileKey;

  private final String mProfile;
  private final File mProfileFile;
  private final IniFile mProfileIni;

  public WiimoteProfileSetting(String gameID, int padID, String section, String key,
          String defaultValue)
  {
    mPadID = padID;
    mSection = section;
    mKey = key;
    mDefaultValue = defaultValue;

    mProfileKey = SettingsFile.KEY_WIIMOTE_PROFILE + (mPadID + 1);

    mProfile = gameID + "_Wii" + padID;
    mProfileFile = SettingsFile.getWiiProfile(mProfile);
    mProfileIni = loadProfile();
  }

  @Override
  public boolean isOverridden(Settings settings)
  {
    return isProfileEnabled(settings);
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return false;
  }

  @Override
  public boolean delete(Settings settings)
  {
    return disableProfile(settings);
  }

  @Override
  public String getString(Settings settings)
  {
    if (isProfileEnabled(settings))
      return mProfileIni.getString(mSection, mKey, mDefaultValue);
    else
      return mDefaultValue;
  }

  @Override
  public void setString(Settings settings, String newValue)
  {
    mProfileIni.setString(mSection, mKey, newValue);

    mProfileIni.save(mProfileFile);
    enableProfile(settings);
  }

  private IniFile loadProfile()
  {
    IniFile profileIni = new IniFile();

    if (!profileIni.load(mProfileFile, false))
    {
      String defaultWiiProfilePath = DirectoryInitialization.getUserDirectory() +
              "/Config/Profiles/Wiimote/WiimoteProfile.ini";

      profileIni.load(defaultWiiProfilePath, false);

      profileIni.setString(Settings.SECTION_PROFILE, "Device",
              "Android/" + (mPadID + 4) + "/Touchscreen");
    }

    return profileIni;
  }

  private void enableProfile(Settings settings)
  {
    getControlsSection(settings).setString(mProfileKey, mProfile);
  }

  private boolean disableProfile(Settings settings)
  {
    return getControlsSection(settings).delete(mProfileKey);
  }

  private boolean isProfileEnabled(Settings settings)
  {
    return mProfile.equals(getControlsSection(settings).getString(mProfileKey, ""));
  }

  private IniFile.Section getControlsSection(Settings s)
  {
    return s.getSection(Settings.GAME_SETTINGS_PLACEHOLDER_FILE_NAME, Settings.SECTION_CONTROLS);
  }
}
