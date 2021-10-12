// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

public class LegacyStringSetting extends AbstractLegacySetting implements AbstractStringSetting
{
  private final String mDefaultValue;

  public LegacyStringSetting(String file, String section, String key, String defaultValue)
  {
    super(file, section, key);
    mDefaultValue = defaultValue;
  }

  @Override
  public String getString(Settings settings)
  {
    return settings.getSection(mFile, mSection).getString(mKey, mDefaultValue);
  }

  @Override
  public void setString(Settings settings, String newValue)
  {
    settings.getSection(mFile, mSection).setString(mKey, newValue);
  }
}
