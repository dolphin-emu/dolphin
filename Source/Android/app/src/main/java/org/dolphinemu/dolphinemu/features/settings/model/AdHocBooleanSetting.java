// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

public class AdHocBooleanSetting implements AbstractBooleanSetting
{
  private final String mFile;
  private final String mSection;
  private final String mKey;
  private final boolean mDefaultValue;

  public AdHocBooleanSetting(String file, String section, String key, boolean defaultValue)
  {
    mFile = file;
    mSection = section;
    mKey = key;
    mDefaultValue = defaultValue;

    if (!NativeConfig.isSettingSaveable(file, section, key))
    {
      throw new IllegalArgumentException("File/section/key is unknown or legacy");
    }
  }

  @Override
  public boolean isOverridden(Settings settings)
  {
    return NativeConfig.isOverridden(mFile, mSection, mKey);
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return true;
  }

  @Override
  public boolean delete(Settings settings)
  {
    return NativeConfig.deleteKey(settings.getWriteLayer(), mFile, mSection, mKey);
  }

  @Override
  public boolean getBoolean(Settings settings)
  {
    return NativeConfig.getBoolean(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey, mDefaultValue);
  }

  @Override
  public void setBoolean(Settings settings, boolean newValue)
  {
    NativeConfig.setBoolean(settings.getWriteLayer(), mFile, mSection, mKey, newValue);
  }

  public static boolean getBooleanGlobal(String file, String section, String key,
          boolean defaultValue)
  {
    return NativeConfig.getBoolean(NativeConfig.LAYER_ACTIVE, file, section, key, defaultValue);
  }
}
