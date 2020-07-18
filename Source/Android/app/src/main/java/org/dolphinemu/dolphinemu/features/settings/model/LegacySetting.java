package org.dolphinemu.dolphinemu.features.settings.model;

public class LegacySetting implements StringSetting, BooleanSetting, IntSetting, FloatSetting
{
  private final String mFile;
  private final String mSection;
  private final String mKey;

  public LegacySetting(String file, String section, String key)
  {
    mFile = file;
    mSection = section;
    mKey = key;
  }

  @Override
  public boolean delete(Settings settings)
  {
    return settings.getSection(mFile, mSection).delete(mKey);
  }

  @Override
  public String getString(Settings settings, String defaultValue)
  {
    return settings.getSection(mFile, mSection).getString(mKey, defaultValue);
  }

  @Override
  public boolean getBoolean(Settings settings, boolean defaultValue)
  {
    return settings.getSection(mFile, mSection).getBoolean(mKey, defaultValue);
  }

  @Override
  public int getInt(Settings settings, int defaultValue)
  {
    return settings.getSection(mFile, mSection).getInt(mKey, defaultValue);
  }

  @Override
  public float getFloat(Settings settings, float defaultValue)
  {
    return settings.getSection(mFile, mSection).getFloat(mKey, defaultValue);
  }

  @Override
  public void setString(Settings settings, String newValue)
  {
    settings.getSection(mFile, mSection).setString(mKey, newValue);
  }

  @Override
  public void setBoolean(Settings settings, boolean newValue)
  {
    settings.getSection(mFile, mSection).setBoolean(mKey, newValue);
  }

  @Override
  public void setInt(Settings settings, int newValue)
  {
    settings.getSection(mFile, mSection).setInt(mKey, newValue);
  }

  @Override
  public void setFloat(Settings settings, float newValue)
  {
    settings.getSection(mFile, mSection).setFloat(mKey, newValue);
  }
}
