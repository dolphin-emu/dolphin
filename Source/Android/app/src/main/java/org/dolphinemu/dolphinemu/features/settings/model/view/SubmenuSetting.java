package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SubmenuSetting extends SettingsItem
{
  private MenuTag mMenuKey;

  public SubmenuSetting(String key, Setting setting, int titleId, int descriptionId,
    MenuTag menuKey)
  {
    super(key, null, setting, titleId, descriptionId);
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
}
