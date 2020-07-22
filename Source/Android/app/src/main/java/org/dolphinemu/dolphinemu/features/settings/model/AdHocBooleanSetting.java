package org.dolphinemu.dolphinemu.features.settings.model;

public class AdHocBooleanSetting extends AbstractLegacySetting implements AbstractBooleanSetting
{
  private final boolean mDefaultValue;

  public AdHocBooleanSetting(String file, String section, String key, boolean defaultValue)
  {
    super(file, section, key);
    mDefaultValue = defaultValue;

    if (!NativeConfig.isSettingSaveable(file, section, key))
    {
      throw new IllegalArgumentException("File/section/key is unknown or legacy");
    }
  }

  @Override
  public boolean delete(Settings settings)
  {
    if (!settings.isGameSpecific())
    {
      return NativeConfig.deleteKey(NativeConfig.LAYER_BASE_OR_CURRENT, mFile, mSection, mKey);
    }
    else
    {
      return settings.getSection(mFile, mSection).delete(mKey);
    }
  }

  @Override
  public boolean getBoolean(Settings settings)
  {
    if (!settings.isGameSpecific())
    {
      return NativeConfig.getBoolean(NativeConfig.LAYER_BASE_OR_CURRENT, mFile, mSection, mKey,
              mDefaultValue);
    }
    else
    {
      return settings.getSection(mFile, mSection).getBoolean(mKey, mDefaultValue);
    }
  }

  @Override
  public void setBoolean(Settings settings, boolean newValue)
  {
    if (!settings.isGameSpecific())
    {
      NativeConfig.setBoolean(NativeConfig.LAYER_BASE_OR_CURRENT, mFile, mSection, mKey, newValue);
    }
    else
    {
      settings.getSection(mFile, mSection).setBoolean(mKey, newValue);
    }
  }
}
