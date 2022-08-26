// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public class InputStringSetting extends SettingsItem
{
  private AbstractStringSetting mSetting;

  private MenuTag mMenuTag;

  public InputStringSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, MenuTag menuTag)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
    mMenuTag = menuTag;
  }

  public InputStringSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId)
  {
    this(context, setting, titleId, descriptionId, null);
  }

  public InputStringSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, int choicesId, int valuesId, MenuTag menuTag)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
    mMenuTag = menuTag;
  }

  public InputStringSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, int choicesId, int valuesId)
  {
    this(context, setting, titleId, descriptionId, choicesId, valuesId, null);
  }

  public String getSelectedValue(Settings settings)
  {
    return mSetting.getString(settings);
  }

  public MenuTag getMenuTag()
  {
    return mMenuTag;
  }

  public void setSelectedValue(Settings settings, String selection)
  {
    mSetting.setString(settings, selection);
  }

  @Override
  public int getType()
  {
    return TYPE_STRING;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
