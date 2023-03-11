// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import androidx.annotation.NonNull;

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
  public boolean isOverridden()
  {
    return NativeConfig.isOverridden(mFile, mSection, mKey);
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return NativeConfig.isSettingSaveable(mFile, mSection, mKey);
  }

  @Override
  public boolean delete(@NonNull Settings settings)
  {
    if (!NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      throw new UnsupportedOperationException(
              "Unsupported setting: " + mFile + ", " + mSection + ", " + mKey);
    }

    return NativeConfig.deleteKey(settings.getWriteLayer(), mFile, mSection, mKey);
  }

  @Override
  public float getFloat()
  {
    return NativeConfig.getFloat(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey, mDefaultValue);
  }

  @Override
  public void setFloat(@NonNull Settings settings, float newValue)
  {
    if (!NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      throw new UnsupportedOperationException(
              "Unsupported setting: " + mFile + ", " + mSection + ", " + mKey);
    }

    NativeConfig.setFloat(settings.getWriteLayer(), mFile, mSection, mKey, newValue);
  }

  public void setFloat(int layer, float newValue)
  {
    NativeConfig.setFloat(layer, mFile, mSection, mKey, newValue);
  }
}
