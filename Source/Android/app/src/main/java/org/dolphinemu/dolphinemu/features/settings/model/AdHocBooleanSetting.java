// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import androidx.annotation.NonNull;

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
  public boolean isOverridden()
  {
    return NativeConfig.isOverridden(mFile, mSection, mKey);
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return true;
  }

  @Override
  public boolean delete(@NonNull Settings settings)
  {
    return NativeConfig.deleteKey(settings.getWriteLayer(), mFile, mSection, mKey);
  }

  @Override
  public boolean getBoolean()
  {
    return NativeConfig.getBoolean(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey, mDefaultValue);
  }

  @Override
  public void setBoolean(@NonNull Settings settings, boolean newValue)
  {
    NativeConfig.setBoolean(settings.getWriteLayer(), mFile, mSection, mKey, newValue);
  }

  public static boolean getBooleanGlobal(String file, String section, String key,
          boolean defaultValue)
  {
    return NativeConfig.getBoolean(NativeConfig.LAYER_ACTIVE, file, section, key, defaultValue);
  }
}
