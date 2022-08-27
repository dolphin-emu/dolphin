// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public class GraphicsModsDisabledWarningFragment extends SettingDisabledWarningFragment
{
  public GraphicsModsDisabledWarningFragment()
  {
    super(BooleanSetting.GFX_MODS_ENABLE, MenuTag.ADVANCED_GRAPHICS,
            R.string.gfx_mods_disabled_warning);
  }
}
