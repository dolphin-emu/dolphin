// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.AdHocBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class LogCheckBoxSetting extends CheckBoxSetting
{
  String mKey;

  public LogCheckBoxSetting(String key, CharSequence title, CharSequence description)
  {
    super(new AdHocBooleanSetting(Settings.FILE_LOGGER, Settings.SECTION_LOGGER_LOGS, key, false),
            title, description);
    mKey = key;
  }

  public String getKey()
  {
    return mKey;
  }
}
