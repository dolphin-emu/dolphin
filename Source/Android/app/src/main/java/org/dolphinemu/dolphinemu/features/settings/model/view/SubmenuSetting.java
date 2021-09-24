// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SubmenuSetting extends SettingsItem
{
  private MenuTag mMenuKey;

  public SubmenuSetting(Context context, int titleId, MenuTag menuKey)
  {
    super(context, titleId, 0);
    mMenuKey = menuKey;
  }

  public MenuTag getMenuKey()
  {
    return mMenuKey;
  }

  @Override
  public int getType()
  {
    return TYPE_SUBMENU;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return null;
  }
}
