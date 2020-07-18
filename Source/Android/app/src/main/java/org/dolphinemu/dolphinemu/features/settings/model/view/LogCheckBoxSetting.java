package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.LegacySetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class LogCheckBoxSetting extends CheckBoxSetting
{
  String mKey;

  public LogCheckBoxSetting(String key, int titleId, int descriptionId,
          boolean defaultValue)
  {
    super(new LegacySetting(Settings.FILE_LOGGER, Settings.SECTION_LOGGER_LOGS, key), titleId,
            descriptionId, defaultValue);
    mKey = key;
  }

  public String getKey()
  {
    return mKey;
  }
}
