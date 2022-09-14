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

  private String[] mChoices;
  private String[] mValues;
  private MenuTag mMenuTag;

  public StringSingleChoiceSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, String[] choices, String[] values, MenuTag menuTag)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
    mChoices = choices;
    mValues = values;
    mMenuTag = menuTag;
  }

  public StringSingleChoiceSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, String[] choices, String[] values)
  {
    this(context, setting, titleId, descriptionId, choices, values, null);
  }

  public StringSingleChoiceSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, int choicesId, int valuesId, MenuTag menuTag)
  {
    super(context, titleId, descriptionId);
    mSetting = setting;
    mChoices = DolphinApplication.getAppContext().getResources().getStringArray(choicesId);
    mValues = DolphinApplication.getAppContext().getResources().getStringArray(valuesId);
    mMenuTag = menuTag;
  }

  public StringSingleChoiceSetting(Context context, AbstractStringSetting setting, int titleId,
          int descriptionId, int choicesId, int valuesId)
  {
    this(context, setting, titleId, descriptionId, choicesId, valuesId, null);
  }

  public String[] getChoices()
  {
    return mChoices;
  }

  public String[] getValues()
  {
    return mValues;
  }

  public String getValueAt(int index)
  {
    if (mValues == null)
      return null;

    if (index >= 0 && index < mValues.length)
    {
      return mValues[index];
    }

    return "";
  }

  public String getSelectedValue(Settings settings)
  {
    return mSetting.getString(settings);
  }

  public int getSelectedValueIndex(Settings settings)
  {
    String selectedValue = getSelectedValue(settings);
    for (int i = 0; i < mValues.length; i++)
    {
      if (mValues[i].equals(selectedValue))
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


