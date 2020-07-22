package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.AdHocBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class LogCheckBoxSetting extends CheckBoxSetting
{
  String mKey;

  public LogCheckBoxSetting(String key, int titleId, int descriptionId)
  {
    super(new AdHocBooleanSetting(Settings.FILE_LOGGER, Settings.SECTION_LOGGER_LOGS, key, false),
            titleId, descriptionId);
    mKey = key;
  }

  public String getKey()
  {
    return mKey;
  }
}
