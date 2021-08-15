// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

public class LegacyBooleanSetting extends AbstractLegacySetting implements AbstractBooleanSetting
{
  private final boolean mDefaultValue;

  public LegacyBooleanSetting(String file, String section, String key, boolean defaultValue)
  {
    super(file, section, key);
    mDefaultValue = defaultValue;
  }

  @Override
  public boolean getBoolean(Settings settings)
  {
    return settings.getSection(mFile, mSection).getBoolean(mKey, mDefaultValue);
  }

  @Override
  public void setBoolean(Settings settings, boolean newValue)
  {
    settings.getSection(mFile, mSection).setBoolean(mKey, newValue);
  }
}
