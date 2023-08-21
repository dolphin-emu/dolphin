// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public final class SingleChoiceSettingDynamicDescriptions extends SettingsItem
{
  private AbstractIntSetting mSetting;

  private int mChoicesId;
  private int mValuesId;
  private int mDescriptionChoicesId;
  private int mDescriptionValuesId;
  private MenuTag menuTag;

  public SingleChoiceSettingDynamicDescriptions(Context context, AbstractIntSetting setting,
          int titleId, int descriptionId, int choicesId, int valuesId, int descriptionChoicesId,
          int descriptionValuesId, MenuTag menuTag)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
    mValuesId = valuesId;
    mChoicesId = choicesId;
    mDescriptionChoicesId = descriptionChoicesId;
    mDescriptionValuesId = descriptionValuesId;
    this.menuTag = menuTag;
  }

  public SingleChoiceSettingDynamicDescriptions(Context context, AbstractIntSetting setting,
          int titleId, int descriptionId, int choicesId, int valuesId, int descriptionChoicesId,
          int descriptionValuesId)
  {
    this(context, setting, titleId, descriptionId, choicesId, valuesId, descriptionChoicesId,
            descriptionValuesId, null);
  }

  public int getChoicesId()
  {
    return mChoicesId;
  }

  public int getValuesId()
  {
    return mValuesId;
  }

  public int getDescriptionChoicesId()
  {
    return mDescriptionChoicesId;
  }

  public int getDescriptionValuesId()
  {
    return mDescriptionValuesId;
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
    return TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}
