// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public class StringSingleChoiceSetting extends SettingsItem
{
  private AbstractStringSetting mSetting;

  private String[] mChoicesId;
  private String[] mValuesId;
  private MenuTag mMenuTag;

  public StringSingleChoiceSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, String[] choicesId, String[] valuesId, MenuTag menuTag)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
    mChoicesId = choicesId;
    mValuesId = valuesId;
    mMenuTag = menuTag;
  }

  public StringSingleChoiceSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, String[] choicesId, String[] valuesId)
  {
    this(context, setting, titleId, descriptionId, choicesId, valuesId, null);
  }

  public StringSingleChoiceSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, int choicesId, int valuesId, MenuTag menuTag)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
    mChoicesId = DolphinApplication.getAppContext().getResources().getStringArray(choicesId);
    mValuesId = DolphinApplication.getAppContext().getResources().getStringArray(valuesId);
    mMenuTag = menuTag;
  }

  public StringSingleChoiceSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, int choicesId, int valuesId)
  {
    this(context, setting, titleId, descriptionId, choicesId, valuesId, null);
  }

  public String[] getChoicesId()
  {
    return mChoicesId;
  }

  public String[] getValuesId()
  {
    return mValuesId;
  }

  public String getValueAt(int index)
  {
    if (mValuesId == null)
      return null;

    if (index >= 0 && index < mValuesId.length)
    {
      return mValuesId[index];
    }

    return "";
  }

  public String getSelectedValue(Settings settings)
  {
    return mSetting.getString(settings);
  }

  public int getSelectValueIndex(Settings settings)
  {
    String selectedValue = getSelectedValue(settings);
    for (int i = 0; i < mValuesId.length; i++)
    {
      if (mValuesId[i].equals(selectedValue))
      {
        return i;
      }
    }

    return -1;
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
    return TYPE_STRING_SINGLE_CHOICE;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return mSetting;
  }
}


