// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

public class LegacyIntSetting extends AbstractLegacySetting implements AbstractIntSetting
{
  private final int mDefaultValue;

  public LegacyIntSetting(String file, String section, String key, int defaultValue)
  {
    super(file, section, key);
    mDefaultValue = defaultValue;
  }

  @Override
  public int getInt(Settings settings)
  {
    return settings.getSection(mFile, mSection).getInt(mKey, mDefaultValue);
  }

  @Override
  public void setInt(Settings settings, int newValue)
  {
    settings.getSection(mFile, mSection).setInt(mKey, newValue);
  }
}
