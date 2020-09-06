package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class FilePicker extends SettingsItem
{
  private String mDefaultValue;
  private int mRequestType;

  public FilePicker(String file, String section, String key, int titleId, int descriptionId,
          String defaultVault, int requestType)
  {
    super(file, section, key, titleId, descriptionId);
    mDefaultValue = defaultVault;
    mRequestType = requestType;
  }

  public String getSelectedValue(Settings settings)
  {
    return settings.getSection(getFile(), getSection()).getString(getKey(), mDefaultValue);
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
