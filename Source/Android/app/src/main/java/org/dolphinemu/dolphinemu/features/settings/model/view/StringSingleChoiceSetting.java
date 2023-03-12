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
  private final AbstractStringSetting mSetting;

  protected String[] mChoices;
  protected String[] mValues;
  private final MenuTag mMenuTag;
  private int mNoChoicesAvailableString = 0;

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
          int descriptionId, String[] choices, String[] values, int noChoicesAvailableString)
  {
    this(context, setting, titleId, descriptionId, choices, values, null);
    mNoChoicesAvailableString = noChoicesAvailableString;
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

  public String getChoiceAt(int index)
  {
    if (mChoices == null)
      return null;

    if (index >= 0 && index < mChoices.length)
    {
      return mChoices[index];
    }

    return "";
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

  public String getSelectedChoice(Settings settings)
  {
    return getChoiceAt(getSelectedValueIndex(settings));
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

  public int getNoChoicesAvailableString()
  {
    return mNoChoicesAvailableString;
  }

  public void setSelectedValue(Settings settings, String selection)
  {
    mSetting.setString(settings, selection);
  }

  public void refreshChoicesAndValues()
  {
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


