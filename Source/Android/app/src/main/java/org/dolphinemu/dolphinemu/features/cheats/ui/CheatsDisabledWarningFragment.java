// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public class CheatsDisabledWarningFragment extends SettingDisabledWarningFragment
{
  public CheatsDisabledWarningFragment()
  {
    super(BooleanSetting.MAIN_ENABLE_CHEATS, MenuTag.CONFIG_GENERAL,
            R.string.cheats_disabled_warning);
  }
}
