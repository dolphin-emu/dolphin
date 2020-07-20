package org.dolphinemu.dolphinemu.features.settings.model;

public enum FloatSetting implements AbstractFloatSetting
{
  // These entries have the same names and order as in C++, just for consistency.

  MAIN_EMULATION_SPEED(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "EmulationSpeed", 1.0f),
  MAIN_OVERCLOCK(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "Overclock", 1.0f);

  private final String mFile;
  private final String mSection;
  private final String mKey;
  private final float mDefaultValue;

  FloatSetting(String file, String section, String key, float defaultValue)
  {
    mFile = file;
    mSection = section;
    mKey = key;
    mDefaultValue = defaultValue;
  }

  @Override
  public boolean delete(Settings settings)
  {
    return settings.getSection(mFile, mSection).delete(mKey);
  }

  @Override
  public float getFloat(Settings settings)
  {
    return settings.getSection(mFile, mSection).getFloat(mKey, mDefaultValue);
  }

  @Override
  public void setFloat(Settings settings, float newValue)
  {
    settings.getSection(mFile, mSection).setFloat(mKey, newValue);
  }
}
