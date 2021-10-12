// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SingleChoiceSetting extends SettingsItem
{
  private AbstractIntSetting mSetting;

  private int mChoicesId;
  private int mValuesId;
  private MenuTag menuTag;

  public SingleChoiceSetting(Context context, AbstractIntSetting setting, int titleId,
          int descriptionId, int choicesId, int valuesId, MenuTag menuTag)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
    mValuesId = valuesId;
    mChoicesId = choicesId;
    this.menuTag = menuTag;
  }

  public SingleChoiceSetting(Context context, AbstractIntSetting setting, int titleId,
          int descriptionId, int choicesId, int valuesId)
  {
    this(context, setting, titleId, descriptionId, choicesId, valuesId, null);
  }

  public int getChoicesId()
  {
    return mChoicesId;
  }

  public int getValuesId()
  {
    return mValuesId;
  }

  public int getSelectedValue(Settings settings)
  {
    return mSetting.getInt(settings);
  }

  public MenuTag getMenuTag()
  {
    return menuTag;
  }

  public void setSelectedValue(Settings settings, int selection)
  {
    mSetting.setInt(settings, selection);
  }

  @Override
  public int getType()
  {
    return TYPE_SINGLE_CHOICE;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
