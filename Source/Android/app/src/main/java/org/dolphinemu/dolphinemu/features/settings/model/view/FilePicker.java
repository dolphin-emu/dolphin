package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;

public final class FilePicker extends SettingsItem
{
  private StringSetting mSetting;
  private String mDefaultValue;
  private int mRequestType;

  public FilePicker(StringSetting setting, int titleId, int descriptionId,
          String defaultVault, int requestType)
  {
    super(titleId, descriptionId);
    mSetting = setting;
    mDefaultValue = defaultVault;
    mRequestType = requestType;
  }

  public String getSelectedValue(Settings settings)
  {
    return mSetting.getString(settings, mDefaultValue);
  }

  public void setSelectedValue(Settings settings, String selection)
  {
    mSetting.setString(settings, selection);
  }

  public int getRequestType()
  {
    return mRequestType;
  }

  @Override
  public int getType()
  {
    return TYPE_FILE_PICKER;
  }
}
