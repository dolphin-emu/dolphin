// SPDX-License-Identifier: GPL-2.0-or-later

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
  public boolean isOverridden(Settings settings)
  {
    if (settings.isGameSpecific() && !NativeConfig.isSettingSaveable(mFile, mSection, mKey))
      return settings.getSection(mFile, mSection).exists(mKey);
    else
      return NativeConfig.isOverridden(mFile, mSection, mKey);
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return NativeConfig.isSettingSaveable(mFile, mSection, mKey);
  }

  @Override
  public boolean delete(Settings settings)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      return NativeConfig.deleteKey(settings.getWriteLayer(), mFile, mSection, mKey);
    }
    else
    {
      return settings.getSection(mFile, mSection).delete(mKey);
    }
  }

  @Override
  public float getFloat(Settings settings)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      return NativeConfig.getFloat(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey, mDefaultValue);
    }
    else
    {
      return settings.getSection(mFile, mSection).getFloat(mKey, mDefaultValue);
    }
  }

  @Override
  public void setFloat(Settings settings, float newValue)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      NativeConfig.setFloat(settings.getWriteLayer(), mFile, mSection, mKey, newValue);
    }
    else
    {
      settings.getSection(mFile, mSection).setFloat(mKey, newValue);
    }
  }

  public float getFloatGlobal()
  {
    return NativeConfig.getFloat(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey, mDefaultValue);
  }

  public void setFloatGlobal(int layer, float newValue)
  {
    NativeConfig.setFloat(layer, mFile, mSection, mKey, newValue);
  }
}
