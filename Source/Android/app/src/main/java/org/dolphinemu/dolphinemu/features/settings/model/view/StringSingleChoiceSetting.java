package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;

public class StringSingleChoiceSetting extends SettingsItem
{
  private String mDefaultValue;

  private String[] mChoicesId;
  private String[] mValuesId;
  private MenuTag mMenuTag;

  public StringSingleChoiceSetting(String file, String section, String key, int titleId,
          int descriptionId, String[] choicesId, String[] valuesId, String defaultValue,
          MenuTag menuTag)
  {
    super(file, section, key, titleId, descriptionId);
    mChoicesId = choicesId;
    mValuesId = valuesId;
    mDefaultValue = defaultValue;
    mMenuTag = menuTag;
  }

  public StringSingleChoiceSetting(String file, String section, String key, int titleId,
          int descriptionId, String[] choicesId, String[] valuesId, String defaultValue)
  {
    this(file, section, key, titleId, descriptionId, choicesId, valuesId, defaultValue, null);
  }

  public StringSingleChoiceSetting(String file, String section, String key, int titleId,
          int descriptionId, int choicesId, int valuesId, String defaultValue, MenuTag menuTag)
  {
    super(file, section, key, titleId, descriptionId);
    mChoicesId = DolphinApplication.getAppContext().getResources().getStringArray(choicesId);
    mValuesId = DolphinApplication.getAppContext().getResources().getStringArray(valuesId);
    mDefaultValue = defaultValue;
    mMenuTag = menuTag;
  }

  public StringSingleChoiceSetting(String file, String section, String key, int titleId,
          int descriptionId, int choicesId, int valuesId, String defaultValue)
  {
    this(file, section, key, titleId, descriptionId, choicesId, valuesId, defaultValue, null);
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
    return settings.getSection(getFile(), getSection()).getString(getKey(), mDefaultValue);
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
    settings.getSection(getFile(), getSection()).setString(getKey(), selection);
  }

  @Override
  public int getType()
  {
    return TYPE_STRING_SINGLE_CHOICE;
  }
}


